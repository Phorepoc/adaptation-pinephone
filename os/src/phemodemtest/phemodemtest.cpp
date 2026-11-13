// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phemodemtest.cpp -- OURS (2026-10-17, modem rung M1): is the EG25-G alive on
// UART3? Opens RBusDevComm unit 3 (the "Comm.A64" PDD built with MODEM=1: opening it runs the kernel's bounded power-up
// sequence, the modem's STATUS pin reads as KSignalDSR), waits for STATUS, probes "AT" until OK, runs
// the Zephyr EG2x-G init script, AT+CFUN=1, AT+CPIN?, AT+CSQ, AT+QENG, AT+COPS?, then AT+QPOWD=1 and waits for STATUS to drop before closing (the close cuts VBAT only then).
// Every step is bounded. SSM_TEST=phemodemtest stages it as ssmtest.exe. Output: "PH: modem ..." lines.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>

_LIT(KLdd, "ECOMM");
static const TInt KModemUnit = 3;

static TInt Ms(const TTime& aT0)
	{
	TTime t; t.UniversalTime();
	return (TInt)(t.MicroSecondsFrom(aT0).Int64() / 1000);
	}

// one bounded read: whatever arrives within aUs
static TInt ReadFor(RBusDevComm& aPort, TDes8& aBuf, TInt aUs)
	{
	TRequestStatus rs, ts;
	RTimer t;
	t.CreateLocal();
	aBuf.Zero();
	aPort.ReadOneOrMore(rs, aBuf);
	t.After(ts, aUs);
	User::WaitForRequest(rs, ts);
	if (rs == KRequestPending)
		{
		aPort.ReadCancel();
		User::WaitForRequest(rs);
		}
	else
		{
		t.Cancel();
		User::WaitForRequest(ts);
		}
	t.Close();
	return rs.Int() == KErrNone || rs.Int() == KErrCancel ? aBuf.Length() : rs.Int();
	}

static void LogLines(const TDesC8& aText, const char* aTag)
	{
	TPtrC8 rest(aText);
	while (rest.Length())
		{
		TInt e = rest.Locate('\n');
		TPtrC8 line = e < 0 ? rest : rest.Left(e);
		rest.Set(e < 0 ? TPtrC8() : rest.Mid(e + 1));
		while (line.Length() && (line[line.Length() - 1] == '\r' || line[line.Length() - 1] == '\n'))
			line.Set(line.Left(line.Length() - 1));
		if (line.Length())
			{
			TBuf8<200> l(line.Left(Min(line.Length(), 199)));
			RDebug::Printf("PH: modem %s %S", aTag, &l);
			}
		}
	}

// send aCmd + CR, collect the reply until OK / ERROR / aDone (or aTimeoutMs); returns KErrNone on OK/aDone; the reply text in *aReply
static TInt Command(RBusDevComm& aPort, const TDesC8& aCmd, TInt aTimeoutMs, const TDesC8& aDone = KNullDesC8,
					 TDes8* aReply = NULL)
	{
	TBuf8<64> out(aCmd);
	out.Append('\r');
	TRequestStatus ws;
	aPort.Write(ws, out);
	User::WaitForRequest(ws);
	TBuf8<64> tx(aCmd);
	RDebug::Printf("PH: modem >> %S (write %d)", &tx, ws.Int());
	TBuf8<1024> acc;
	TBuf8<256> buf;
	TTime t0; t0.UniversalTime();
	TInt r = KErrTimedOut;
	while (Ms(t0) < aTimeoutMs)
		{
		TInt n = ReadFor(aPort, buf, 200000);
		if (n < 0)												// a line error (KErrCommsBreak -48 = RX held low, e.g.
			{														// by an unpowered modem): log, keep reading
			RDebug::Printf("PH: modem read error %d", n);
			r = n;
			continue;
			}
		if (n == 0)
			continue;
		if (acc.Length() + n > acc.MaxLength())
			{
			LogLines(acc, "<<");
			acc.Zero();
			}
		acc.Append(buf);
		if (acc.Find(_L8("\r\nOK\r\n")) >= 0 || acc.Find(_L8("OK\r\n")) == 0 || (aDone.Length() && acc.Find(aDone) >= 0))
			{ r = KErrNone; break; }
		if (acc.Find(_L8("ERROR")) >= 0)
			{ r = KErrGeneral; break; }
		}
	LogLines(acc, "<<");
	if (aReply)
		aReply->Copy(acc.Left(Min(acc.Length(), aReply->MaxLength())));
	RDebug::Printf("PH: modem %S -> %d after %d ms", &tx, r, Ms(t0));
	return r;
	}

// read (and log) unsolicited lines until every marker in aWant was seen or aTimeoutMs; returns the seen-mask
static TUint WaitUrcs(RBusDevComm& aPort, const TDesC8* const* aWant, TInt aCount, TInt aTimeoutMs, TUint aSeen = 0)
	{
	TBuf8<256> buf;
	TBuf8<512> acc;
	const TUint all = (1u << aCount) - 1;
	TTime t0; t0.UniversalTime();
	while ((aSeen & all) != all && Ms(t0) < aTimeoutMs)
		{
		const TInt n = ReadFor(aPort, buf, 200000);
		if (n <= 0)
			continue;
		LogLines(buf, "<<");
		if (acc.Length() + n > acc.MaxLength())
			acc.Delete(0, acc.Length() + n - acc.MaxLength());
		acc.Append(buf);
		for (TInt i = 0; i < aCount; ++i)
			if (acc.Find(*aWant[i]) >= 0)
				aSeen |= 1u << i;
		}
	RDebug::Printf("PH: modem URC wait: seen mask %x of %x after %d ms", aSeen, all, Ms(t0));
	return aSeen;
	}

// The init script of Zephyr's EG2x-G driver (Apache-2.0; modem-eg25/third_party/zephyr/drivers/modem/
// vendor_modem_cellular/cellular_quectel_eg2x_g.c, quectel_eg2x_g_init_chat_script_cmds), in its order, minus
// AT+IFC=2,2 (only with hardware flow control, which UART3 does not have) and AT+CMUX (no multiplexer yet).
// AT+CFUN=4 in Zephyr's script = RF off until its dial script sends AT+CFUN=1; done the same way here.
static const char* const KZephyrInit[] =
	{
	"ATE0", "AT+CFUN=4", "AT+CMEE=1", "AT+CREG=1", "AT+CGREG=1", "AT+CEREG=1",
	"AT+CREG?", "AT+CEREG?", "AT+CGREG?", "AT+CGSN", "AT+CGMM", "AT+CGMI", "AT+CGMR", "AT+CIMI",
	};

static TInt ZephyrInit(RBusDevComm& aPort)
	{
	TInt failed = 0;
	for (TUint i = 0; i < sizeof(KZephyrInit) / sizeof(KZephyrInit[0]); ++i)
		if (Command(aPort, TPtrC8((const TUint8*)KZephyrInit[i]), 5000) != KErrNone)
			++failed;
	RDebug::Printf("PH: modem Zephyr EG2x-G init script: %d of %d failed", failed, (TInt)(sizeof(KZephyrInit) / sizeof(KZephyrInit[0])));
	return failed;
	}

static TBool StatusOn(RBusDevComm& aPort)	{ return (aPort.Signals() & KSignalDSR) != 0; }

static TBool WaitStatusOff(RBusDevComm& aPort, TInt aMs)
	{
	TBuf8<256> buf;
	TTime t0; t0.UniversalTime();
	while (Ms(t0) < aMs && StatusOn(aPort))
		if (ReadFor(aPort, buf, 200000) > 0)
			LogLines(buf, "<<");
	return !StatusOn(aPort);
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: modem test start");
	TInt r = User::LoadLogicalDevice(KLdd);
	RDebug::Printf("PH: modem LoadLogicalDevice(ECOMM) -> %d", r);
	RBusDevComm port;
	r = port.Open(KModemUnit);
	RDebug::Printf("PH: modem RBusDevComm::Open(%d) -> %d (needs the MODEM=1 kernel)", KModemUnit, r);
	if (r != KErrNone)
		return r;
	TCommConfig cfg;
	port.Config(cfg);
	cfg().iRate = EBps115200;
	cfg().iDataBits = EData8;
	cfg().iStopBits = EStop1;
	cfg().iParity = EParityNone;
	cfg().iHandshake = KConfigFreeRTS | KConfigFreeDTR;		// the DTS: no usable hardware flow control
	cfg().iTerminatorCount = 0;
	r = port.SetConfig(cfg);
	RDebug::Printf("PH: modem SetConfig 115200 8N1 -> %d", r);

	// 1. STATUS: the kernel reports DSR only once its power-up sequence (~1.3 s) is done AND PH9 is low (on)
	TTime t0; t0.UniversalTime();
	User::After(1500000);
	TBool on = EFalse;
	while (Ms(t0) < 20000 && !(on = StatusOn(port)))
		User::After(100000);
	RDebug::Printf("PH: modem STATUS %s after %d ms", on ? "on" : "still off", Ms(t0));

	// 2. the modem's own start-up URCs: "RDY" (UART up, ~10 s), "+CFUN: 1", "+CPIN: ..." (boot 897: these came
	//    in that order; commands before them got ERROR). Bounded: 40 s.
	port.ResetBuffers();
	_LIT8(KRdy, "RDY"); _LIT8(KCfun, "+CFUN: 1"); _LIT8(KCpin, "+CPIN:");
	const TDesC8* const want[] = { &KRdy, &KCfun, &KCpin };
	TUint seen = WaitUrcs(port, want, 3, 40000);
	TBool alive = EFalse;
	for (TInt i = 0; i < 5 && !alive; ++i)
		alive = Command(port, _L8("AT"), 1000) == KErrNone;
	RDebug::Printf("PH: modem AT %s after %d ms (urcs %x)", alive ? "OK" : "no answer", Ms(t0), seen);
	if (alive)
		{
		ZephyrInit(port);
		if ((seen & 6) != 6)									// RDY came before we listened: give CFUN/CPIN 15 s more
			seen = WaitUrcs(port, want, 3, 15000, seen | 1);
		Command(port, _L8("AT+CFUN=1"), 15000);		// Zephyr's dial script turns RF on this way
		Command(port, _L8("AT+CPIN?"), 5000);
		Command(port, _L8("AT+CSQ"), 3000);			// Zephyr's periodic script: CSQ + QENG serving cell
		Command(port, _L8("AT+QENG=\"servingcell\""), 5000);
		Command(port, _L8("AT+COPS?"), 10000);
		// M5 call audio without a SIM: the codec route on (unit 3 DTR -> a64sound.cpp), the modem's own audio loop
		// (AT+QAUDLOOP=1: its PCM uplink back to the downlink) -> speak into the bottom mic, hear it in the earpiece.
		// (read-only audio probes: the PCM settings are not changed by this test)
		Command(port, _L8("AT+CMEE?"), 2000);
		Command(port, _L8("AT+CFUN?"), 3000);
		Command(port, _L8("AT+QDAI=?"), 3000);
		Command(port, _L8("AT+CLVL?"), 3000);
		Command(port, _L8("AT+CMUT?"), 3000);
		Command(port, _L8("AT+QAUDMOD?"), 3000);
		Command(port, _L8("AT+QSIDET?"), 3000);
		Command(port, _L8("AT+QAUDLOOP?"), 3000);
		Command(port, _L8("AT+QAUDLOOP=?"), 3000);
		TBool dai = EFalse;
		for (TInt i = 0; i < 13 && !dai; ++i)		// boot 983: ERROR 12.7 s after RDY, OK at 17.8 s; up to 60 s
			{
			if (i)
				User::After(5000000);
			dai = Command(port, _L8("AT+QDAI?"), 3000) == KErrNone;
			RDebug::Printf("PH: modem audio probe %d: AT+QDAI? %s (%d ms after RDY wait)", i, dai ? "OK" : "ERROR", Ms(t0));
			}
		port.SetSignals(KSignalDTR, 0);
		User::After(300000);
		Command(port, _L8("AT+QSIDET=20000"), 3000);
		const TInt lr = Command(port, _L8("AT+QAUDLOOP=1"), 3000);
		RDebug::Printf("PH: modem audio loop %s: SPEAK INTO THE MIC, LISTEN AT THE EARPIECE (20 s)", lr == KErrNone ? "ON" : "FAILED");
		User::After(20000000);
		Command(port, _L8("AT+QAUDLOOP=0"), 3000);
		port.SetSignals(0, KSignalDTR);
		RDebug::Printf("PH: modem audio loop off");
		}

	// 3. power-down: AT+QPOWD=1 up to 3 times (boot 897: the first got ERROR, before "+CFUN: 1"), STATUS high within
	//    30 s each. If the modem is still on, closing the port makes the kernel do the PWRKEY off-pulse and, as the
	//    last resort, cut VBAT (A64ModemRelease): a modem is never left on.
	TBool off = !on;
	for (TInt i = 0; i < 3 && !off; ++i)
		{
		const TInt q = Command(port, _L8("AT+QPOWD=1"), 5000, _L8("POWERED DOWN"));
		off = WaitStatusOff(port, q == KErrNone ? 30000 : 3000);
		RDebug::Printf("PH: modem QPOWD try %d -> %d, STATUS %s", i + 1, q, off ? "off" : "on");
		}
	RDebug::Printf("PH: modem STATUS %s before close%s", off ? "off" : "ON", off ? "" : " (the kernel's PWRKEY off-pulse / VBAT cut takes over)");
	port.Close();
	RDebug::Printf("PH: modem test done (alive=%d)", alive);
	return KErrNone;
	}
