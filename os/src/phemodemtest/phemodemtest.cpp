// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phemodemtest.cpp -- OURS (2026-10-17, modem rung M1): is the EG25-G alive on
// UART3? Opens RBusDevComm unit 3 (the "Comm.A64" PDD built with MODEM=1: opening it runs the kernel's bounded power-up
// sequence, the modem's STATUS pin reads as KSignalDSR), waits for STATUS, probes "AT" until OK, runs ATI, AT+CPIN?,
// AT+CSQ, AT+COPS?, then AT+QPOWD=1 and waits for STATUS to drop before closing (the close cuts VBAT only then).
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

// eg25-manager's boot config (data/pine64,pinephone-1.2.toml [at] configure, "query, set if different"), minus the
// GNSS entries (no GPS rung yet). URCs go to all ports ("all") so the UART sees them too.
struct TEnsure { const char* iQuery; const char* iExpect; const char* iSet; };
static const TEnsure KEnsure[] =
	{
	{ "AT+QDAI?", "1,1,0,1,0,0,1,1", "AT+QDAI=1,1,0,1,0,0,1,1" },
	{ "AT+QCFG=\"risignaltype\"", "\"physical\"", "AT+QCFG=\"risignaltype\",\"physical\"" },
	{ "AT+QCFG=\"ims\"", "\"ims\",1", "AT+QCFG=\"ims\",1" },
	{ "AT+QCFG=\"apready\"", "1,0,500", "AT+QCFG=\"apready\",1,0,500" },
	{ "AT+QCFG=\"urc/ri/ring\"", "\"pulse\",120,1000,5000,\"off\",1", "AT+QCFG=\"urc/ri/ring\",\"pulse\",120,1000,5000,\"off\",1" },
	{ "AT+QCFG=\"urc/ri/smsincoming\"", "\"pulse\",120,1", "AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",120,1" },
	{ "AT+QCFG=\"urc/ri/other\"", "\"off\",1,1", "AT+QCFG=\"urc/ri/other\",\"off\",1,1" },
	{ "AT+QCFG=\"urc/delay\"", "\"urc/delay\",0", "AT+QCFG=\"urc/delay\",0" },
	{ "AT+QCFG=\"urc/cache\"", "\"urc/cache\",0", "AT+QCFG=\"urc/cache\",0" },
	{ "AT+QCFG=\"urc/ri/pin\"", "uart_ri", "AT+QCFG=\"urc/ri/pin\",uart_ri" },
	{ "AT+QURCCFG=\"urcport\"", "\"all\"", "AT+QURCCFG=\"urcport\",\"all\"" },
	{ NULL, NULL, "AT+QSCLK=1" },
	{ "AT+QCFG=\"fast/poweroff\"", "\"fast/poweroff\",0", "AT+QCFG=\"fast/poweroff\",0" },
	{ "AT+QCFG=\"sleepind/level\"", "\"sleepind/level\",0", "AT+QCFG=\"sleepind/level\",0" },
	{ "AT+QCFG=\"wakeupin/level\"", "0,0", "AT+QCFG=\"wakeupin/level\",0,0" },
	// (ApRstLevel / ModemRstLevel dropped: boot 983 ERROR on EG25GGBR07A08M2G, query and set; they only choose what a
	// modem crash resets -- not needed for us)
	};

static void EnsureConfig(RBusDevComm& aPort)
	{
	TInt changed = 0, failed = 0;
	TBuf8<1024> reply;
	for (TUint i = 0; i < sizeof(KEnsure) / sizeof(KEnsure[0]); ++i)
		{
		const TEnsure& e = KEnsure[i];
		if (e.iQuery)
			{
			const TPtrC8 q((const TUint8*)e.iQuery), x((const TUint8*)e.iExpect);
			const TInt qr = Command(aPort, q, 3000, KNullDesC8, &reply);
			if (qr == KErrNone && reply.FindF(x) >= 0)
				continue;
			if (qr != KErrNone)
				{
				RDebug::Printf("PH: modem config: %s not supported here (query ERROR), skipped", e.iQuery);
				continue;
				}
			}
		++changed;
		if (Command(aPort, TPtrC8((const TUint8*)e.iSet), 3000) != KErrNone)
			++failed;
		}
	RDebug::Printf("PH: modem config ensured: %d set, %d failed", changed, failed);
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
		Command(port, _L8("ATE0"), 2000);
		Command(port, _L8("AT+CMEE=2"), 2000);
		if ((seen & 6) != 6)									// RDY came before we listened: give CFUN/CPIN 15 s more
			seen = WaitUrcs(port, want, 3, 15000, seen | 1);
		Command(port, _L8("ATI"), 3000);
		Command(port, _L8("AT+CPIN?"), 5000);
		Command(port, _L8("AT+CSQ"), 3000);
		Command(port, _L8("AT+CREG?"), 3000);
		Command(port, _L8("AT+COPS?"), 10000);
		// M5 call audio without a SIM: the codec route on (unit 3 DTR -> a64sound.cpp), the modem's own audio loop
		// (AT+QAUDLOOP=1: its PCM uplink back to the downlink) -> speak into the bottom mic, hear it in the earpiece.
		// AT+QDAI only takes effect after a modem restart: if "config ensured" set it this run, run the test again.
		// boot 982: AT+QDAI? / AT+QSIDET / AT+QAUDLOOP=1 answered a bare ERROR (QAUDLOOP=0 OK) right after the
		// AT block. Probes first (extended errors on; test forms; the generic audio commands; CFUN), then the audio
		// commands retried every 5 s for up to 30 s in case the audio subsystem comes up later than RDY.
		Command(port, _L8("AT+CMEE=2"), 2000);
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
		EnsureConfig(port);						// now: QDAI is only queryable once the audio subsystem is up
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
