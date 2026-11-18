// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phegnsstest.cpp -- OURS (GNSS rung 1): the EG25-G's GNSS over USB.
//   AT port  = COMM::11 (the modem's second AT port; COMM::10 belongs to the telephony TSY)
//   NMEA port = COMM::9 (USB interface 1)
// Sequence: wait for both ports; AT+QGPSCFG="outport" (set "usbnmea" if it is not), AT+QGPS=1; read the NMEA port for
// 60 s and count the sentences by type, checksums and RMC "valid" flags; AT+QGPSEND. No sentence is logged and no
// field but the RMC status is read: a fix (indoors there is none) would carry coordinates. Every wait is bounded.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>
#include <c32comm.h>

_LIT(KLdd, "ECOMM");
_LIT(KCsy, "ECUART");
static const TInt KNmeaUnit = 9;
static const TInt KAtUnit = 11;
static const TInt KReadMs = 60000;

static TInt Ms(const TTime& aT0)
	{
	TTime t; t.UniversalTime();
	return (TInt)(t.MicroSecondsFrom(aT0).Int64() / 1000);
	}

static TInt ReadFor(RComm& aPort, TDes8& aBuf, TInt aUs)
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

static void LogLines(const TDesC8& aText)
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
			TBuf8<160> l(line.Left(Min(line.Length(), 159)));
			RDebug::Printf("PH: gnss AT << %S", &l);
			}
		}
	}

// send aCmd + CR; collect the reply until OK / ERROR or aTimeoutMs; KErrNone on OK. aReply (optional) gets the text.
static TInt Command(RComm& aPort, const char* aCmd, TInt aTimeoutMs, TDes8* aReply = NULL)
	{
	TBuf8<64> cmd((const TUint8*)aCmd);
	TBuf8<64> out(cmd);
	out.Append('\r');
	TRequestStatus ws;
	aPort.Write(ws, out);
	User::WaitForRequest(ws);
	RDebug::Printf("PH: gnss AT >> %S (write %d)", &cmd, ws.Int());
	TBuf8<512> acc;
	TBuf8<256> buf;
	TTime t0; t0.UniversalTime();
	TInt r = KErrTimedOut;
	while (Ms(t0) < aTimeoutMs)
		{
		TInt n = ReadFor(aPort, buf, 200000);
		if (n <= 0)
			continue;
		if (acc.Length() + n > acc.MaxLength())
			break;
		acc.Append(buf);
		if (acc.Find(_L8("OK\r\n")) >= 0)
			{ r = KErrNone; break; }
		if (acc.Find(_L8("ERROR")) >= 0)
			{ r = KErrGeneral; break; }
		}
	LogLines(acc);
	if (aReply)
		aReply->Copy(acc.Left(Min(acc.Length(), aReply->MaxLength())));
	RDebug::Printf("PH: gnss AT %S -> %d after %d ms", &cmd, r, Ms(t0));
	return r;
	}

// ---- the NMEA tally ----
struct TStat { TBuf8<8> iType; TInt iCount; };
static TStat gStat[16];
static TInt gNStat, gValid, gBad, gOther, gRmcValid, gBytes, gReadErrs;

static void Tally(const TDesC8& aType)
	{
	for (TInt i = 0; i < gNStat; ++i)
		if (gStat[i].iType == aType)
			{ ++gStat[i].iCount; return; }
	if (gNStat < 16)
		{
		gStat[gNStat].iType.Copy(aType);
		gStat[gNStat].iCount = 1;
		++gNStat;
		}
	}

static TInt Hex(TUint8 c)
	{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
	}

static void ProcessLine(const TDesC8& aLine)
	{
	if (aLine.Length() < 8 || aLine[0] != '$')
		{
		++gOther;
		return;
		}
	const TInt star = aLine.Locate('*');
	TBool ok = EFalse;
	if (star > 1 && star + 2 < aLine.Length())
		{
		TUint8 x = 0;
		for (TInt i = 1; i < star; ++i)
			x = (TUint8)(x ^ aLine[i]);
		const TInt h = Hex(aLine[star + 1]), l = Hex(aLine[star + 2]);
		ok = h >= 0 && l >= 0 && x == (TUint8)(h * 16 + l);
		}
	if (!ok)
		{
		++gBad;
		return;
		}
	++gValid;
	TBuf8<8> type(aLine.Mid(1, 5));
	Tally(type);
	if (type.Length() == 5 && type[2] == 'R' && type[3] == 'M' && type[4] == 'C')
		{
		// $..RMC,<time>,<status A/V>,...: only the status flag is read
		TInt commas = 0, i = 0;
		for (; i < aLine.Length() && commas < 2; ++i)
			if (aLine[i] == ',')
				++commas;
		if (i < aLine.Length() && aLine[i] == 'A')
			++gRmcValid;
		}
	}

static void ReadNmea(RComm& aPort, TInt aMs)
	{
	TBuf8<512> carry;
	TBuf8<256> buf;
	TTime t0; t0.UniversalTime();
	while (Ms(t0) < aMs)
		{
		TInt n = ReadFor(aPort, buf, 500000);
		if (n < 0)
			{
			++gReadErrs;
			User::After(100000);
			continue;
			}
		if (n == 0)
			continue;
		gBytes += n;
		if (carry.Length() + n > carry.MaxLength())
			carry.Zero();
		carry.Append(buf);
		for (;;)
			{
			const TInt e = carry.Locate('\n');
			if (e < 0)
				break;
			TPtrC8 line(carry.Left(e));
			while (line.Length() && line[line.Length() - 1] == '\r')
				line.Set(line.Left(line.Length() - 1));
			ProcessLine(line);
			carry.Delete(0, e + 1);
			}
		}
	}

static TInt OpenPort(RCommServ& aSrv, RComm& aPort, TInt aUnit)
	{
	TBuf<16> name;
	name.Format(_L("COMM::%d"), aUnit);
	TInt r = aPort.Open(aSrv, name, ECommExclusive);
	if (r != KErrNone)
		return r;
	TCommConfig cfg;
	aPort.Config(cfg);
	cfg().iRate = EBps115200;
	cfg().iDataBits = EData8;
	cfg().iStopBits = EStop1;
	cfg().iParity = EParityNone;
	cfg().iHandshake = 0;
	cfg().iTerminatorCount = 0;
	return aPort.SetConfig(cfg);
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: gnss start");
	User::LoadLogicalDevice(KLdd);
	RCommServ srv;
	TInt r = srv.Connect();
	RDebug::Printf("PH: gnss RCommServ::Connect -> %d", r);
	if (r == KErrNone)
		{
		r = srv.LoadCommModule(KCsy);
		RDebug::Printf("PH: gnss LoadCommModule(ECUART) -> %d (-11 = already loaded)", r);
		r = KErrNone;
		}
	RComm nmea, at;
	TBool nmeaOpen = EFalse, atOpen = EFalse;
	if (r == KErrNone)
		{
		// the EG25 enumerates 15-20 s after PWRKEY (the TSY powers it at boot): poll both ports for 150 s
		TTime w0; w0.UniversalTime();
		while (Ms(w0) < 150000 && !(nmeaOpen && atOpen))
			{
			if (!nmeaOpen && OpenPort(srv, nmea, KNmeaUnit) == KErrNone)
				{
				nmeaOpen = ETrue;
				RDebug::Printf("PH: gnss COMM::%d (NMEA) opened after %d ms", KNmeaUnit, Ms(w0));
				}
			if (!atOpen && OpenPort(srv, at, KAtUnit) == KErrNone)
				{
				atOpen = ETrue;
				RDebug::Printf("PH: gnss COMM::%d (AT) opened after %d ms", KAtUnit, Ms(w0));
				}
			if (!(nmeaOpen && atOpen))
				User::After(500000);
			}
		}
	TBool atOk = EFalse, on = EFalse, off = EFalse;
	if (nmeaOpen && atOpen)
		{
		for (TInt k = 0; k < 3 && !atOk; ++k)
			atOk = Command(at, "AT", 2000) == KErrNone;
		RDebug::Printf("PH: gnss the AT port %s", atOk ? "answers" : "does not answer");
		}
	if (atOk)
		{
		Command(at, "ATE0", 3000);
		TBuf8<200> rep;
		Command(at, "AT+QGPSCFG=\"outport\"", 3000, &rep);
		if (rep.Find(_L8("usbnmea")) < 0)
			Command(at, "AT+QGPSCFG=\"outport\",\"usbnmea\"", 3000);	// persistent in the modem NV (eg25-manager does the same)
		Command(at, "AT+QGPS?", 3000);
		r = Command(at, "AT+QGPS=1", 5000, &rep);
		on = r == KErrNone || rep.Find(_L8("504")) >= 0;			// 504 = session already ongoing
		RDebug::Printf("PH: gnss GNSS %s", on ? "on" : "not started");
		if (on)
			{
			ReadNmea(nmea, KReadMs);
			RDebug::Printf("PH: gnss read %d s: %d bytes, %d valid sentences, %d bad checksums, %d other lines, %d read errors",
						   KReadMs / 1000, gBytes, gValid, gBad, gOther, gReadErrs);
			for (TInt i = 0; i < gNStat; ++i)
				RDebug::Printf("PH: gnss sentences %S = %d", &gStat[i].iType, gStat[i].iCount);
			RDebug::Printf("PH: gnss RMC with a valid fix flag: %d (indoors: expect 0)", gRmcValid);
			}
		off = Command(at, "AT+QGPSEND", 5000) == KErrNone;
		Command(at, "AT+QGPS?", 3000);
		RDebug::Printf("PH: gnss GNSS %s", off ? "off" : "NOT confirmed off");
		}
	if (atOpen)
		at.Close();
	if (nmeaOpen)
		nmea.Close();
	srv.Close();
	const TBool pass = on && off && gValid > 0;
	RDebug::Printf("PH: gnss done (%s)", pass ? "PASS" : "FAIL");
	return pass ? KErrNone : KErrGeneral;
	}
