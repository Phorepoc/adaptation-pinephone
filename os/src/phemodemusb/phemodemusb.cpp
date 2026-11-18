// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phemodemusb.cpp -- OURS (modem step 3): the EG25-G's AT port over USB (COMM::8..15 = the "Comm.USB" PDD, FreeBSD
// ucom + u3g). Unit 3 (UART3) is opened only for its power-up and STATUS pin (RBusDevComm, MODEM=1 kernel) and held
// to the end; the USB serial ports are used through RBusDevComm (C32 + ECUART), the way pheltsy will. Every wait is bounded.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>

_LIT(KLdd, "ECOMM");
static const TInt KPowerUnit = 3;
static const TInt KFirstUsb = 8;
static const TInt KPorts = 8;

static TInt Ms(const TTime& aT0)
	{
	TTime t; t.UniversalTime();
	return (TInt)(t.MicroSecondsFrom(aT0).Int64() / 1000);
	}

static void LogLines(const TDesC8& aText, TInt aUnit)
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
			RDebug::Printf("PH: musb unit %d << %S", aUnit, &l);
			}
		}
	}

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

// send aCmd + CR; collect the reply until OK / ERROR or aTimeoutMs; KErrNone on OK
static TInt Command(RBusDevComm& aPort, TInt aUnit, const char* aCmd, TInt aTimeoutMs)
	{
	TBuf8<64> cmd((const TUint8*)aCmd);
	TBuf8<64> out(cmd);
	out.Append('\r');
	TRequestStatus ws;
	aPort.Write(ws, out);
	User::WaitForRequest(ws);
	RDebug::Printf("PH: musb unit %d >> %S (write %d)", aUnit, &cmd, ws.Int());
	TBuf8<1024> acc;
	TBuf8<256> buf;
	TTime t0; t0.UniversalTime();
	TInt r = KErrTimedOut;
	while (Ms(t0) < aTimeoutMs)
		{
		TInt n = ReadFor(aPort, buf, 200000);
		if (n <= 0)
			continue;
		if (acc.Length() + n > acc.MaxLength())
			{
			LogLines(acc, aUnit);
			acc.Zero();
			}
		acc.Append(buf);
		if (acc.Find(_L8("OK\r\n")) >= 0)
			{ r = KErrNone; break; }
		if (acc.Find(_L8("ERROR")) >= 0)
			{ r = KErrGeneral; break; }
		}
	LogLines(acc, aUnit);
	RDebug::Printf("PH: musb unit %d %S -> %d after %d ms", aUnit, &cmd, r, Ms(t0));
	return r;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: musb start");
	TInt r = User::LoadLogicalDevice(KLdd);
	RDebug::Printf("PH: musb LoadLogicalDevice(ECOMM) -> %d", r);
	RBusDevComm power;
	r = power.Open(KPowerUnit);
	RDebug::Printf("PH: musb power unit %d Open -> %d (the modem power-up; held to the end)", KPowerUnit, r);
	if (r != KErrNone)
		return r;
	TTime t0; t0.UniversalTime();
	User::After(1500000);
	TBool on = EFalse;
	while (Ms(t0) < 20000 && !(on = (power.Signals() & KSignalDSR) != 0))
		User::After(100000);
	RDebug::Printf("PH: musb STATUS %s after %d ms", on ? "on" : "still off", Ms(t0));

	TInt atUnit = -1;
	if (true)
		{
		// the EG25 enumerates 15-20 s after PWRKEY; the ports appear as the stack attaches them: poll COMM::8..15 for 120 s
		TBool tried[KPorts] = { EFalse };
		TTime w0; w0.UniversalTime();
		while (Ms(w0) < 120000 && atUnit < 0)
			{
			for (TInt i = 0; i < KPorts && atUnit < 0; ++i)
				{
				if (tried[i])
					continue;
				RBusDevComm c;
				TInt o = c.Open(KFirstUsb + i);
				if (o != KErrNone)
					continue;
				tried[i] = ETrue;
				RDebug::Printf("PH: musb unit %d opened after %d ms", KFirstUsb + i, Ms(w0));
				TCommConfig cfg;
				c.Config(cfg);
				cfg().iRate = EBps115200;
				cfg().iDataBits = EData8;
				cfg().iStopBits = EStop1;
				cfg().iParity = EParityNone;
				cfg().iHandshake = 0;
				cfg().iTerminatorCount = 0;
				TInt sc = c.SetConfig(cfg);
				RDebug::Printf("PH: musb unit %d SetConfig -> %d", KFirstUsb + i, sc);
				TBool ok = EFalse;
				for (TInt k = 0; k < 3 && !ok; ++k)
					ok = Command(c, KFirstUsb + i, "AT", 2000) == KErrNone;
				if (ok)
					{
					RDebug::Printf("PH: musb unit %d is an AT port", KFirstUsb + i);
					Command(c, KFirstUsb + i, "ATE0", 3000);
					Command(c, KFirstUsb + i, "ATI", 3000);
					Command(c, KFirstUsb + i, "AT+CGMM", 3000);
					Command(c, KFirstUsb + i, "AT+CGMR", 3000);
					Command(c, KFirstUsb + i, "AT+CPIN?", 3000);
					Command(c, KFirstUsb + i, "AT+CSQ", 3000);
					Command(c, KFirstUsb + i, "AT+QCFG=\"usbnet\"", 3000);
					atUnit = KFirstUsb + i;
					Command(c, atUnit, "AT+QPOWD=1", 5000);
					}
				else
					RDebug::Printf("PH: musb unit %d: no AT answer", KFirstUsb + i);
				c.Close();
				}
			User::After(500000);
			}
		RDebug::Printf("PH: musb AT port: %d (-1 = none within 120 s)", atUnit);
		}
	// the close of unit 3 cuts VBAT: only after STATUS dropped (a clean power-down) -- or 30 s
	TTime p0; p0.UniversalTime();
	while (Ms(p0) < 30000 && (power.Signals() & KSignalDSR) != 0)
		User::After(200000);
	RDebug::Printf("PH: musb STATUS %s before close", (power.Signals() & KSignalDSR) ? "still on" : "off");
	power.Close();
	RDebug::Printf("PH: musb done (%s)", atUnit >= 0 ? "PASS" : "FAIL");
	return atUnit >= 0 ? KErrNone : KErrNotFound;
	}
