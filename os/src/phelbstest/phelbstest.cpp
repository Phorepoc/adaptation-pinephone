// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phelbstest.cpp -- OURS (GNSS rung 2): the 2010 location stack end to end on the serial NMEA PSY.
// RPositionServer lists the modules (ours = 0xE7016C20), RPositioner opens it and asks for position updates with partial
// updates accepted: indoors there is no fix, so the answer is KPositionPartialUpdate (satellite information, no position),
// or a time-out. The module status and the satellite counts are logged. NO COORDINATE IS READ OR LOGGED (GetPosition is
// never called). At the end GNSS is confirmed off over the modem's second AT port (COMM::11), and switched off if not.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>
#include <c32comm.h>
#include <lbs.h>
#include <lbssatellite.h>

static const TPositionModuleId KPheModule = { 0xE7016C20 };
static const TInt KRounds = 6;				// up to 6 requests of 25 s: the modem enumerates 15-20 s after power-up

static TInt Ms(const TTime& aT0)
	{
	TTime t; t.UniversalTime();
	return (TInt)(t.MicroSecondsFrom(aT0).Int64() / 1000);
	}

// AT command on COMM::11 (own session): the reply text, KErrNone on OK
static TInt At(RComm& aPort, const char* aCmd, TDes8& aReply)
	{
	TBuf8<64> out((const TUint8*)aCmd);
	out.Append('\r');
	TRequestStatus ws;
	aPort.Write(ws, out);
	User::WaitForRequest(ws);
	aReply.Zero();
	TTime t0; t0.UniversalTime();
	TInt r = KErrTimedOut;
	while (Ms(t0) < 4000)
		{
		TBuf8<128> buf;
		TRequestStatus rs, ts;
		RTimer t; t.CreateLocal();
		aPort.ReadOneOrMore(rs, buf);
		t.After(ts, 200000);
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
		if (buf.Length() && aReply.Length() + buf.Length() <= aReply.MaxLength())
			aReply.Append(buf);
		if (aReply.Find(_L8("OK\r\n")) >= 0)
			{ r = KErrNone; break; }
		if (aReply.Find(_L8("ERROR")) >= 0)
			{ r = KErrGeneral; break; }
		}
	return r;
	}

// GNSS state over the second AT port: 0 off, 1 on, -1 unknown; switches it off when it is on
static TInt EnsureGnssOff()
	{
	RCommServ srv;
	if (srv.Connect() != KErrNone)
		return -1;
	srv.LoadCommModule(_L("ECUART"));
	RComm at;
	TInt state = -1;
	if (at.Open(srv, _L("COMM::11"), ECommExclusive) == KErrNone)
		{
		TCommConfig cfg;
		at.Config(cfg);
		cfg().iRate = EBps115200; cfg().iDataBits = EData8; cfg().iStopBits = EStop1; cfg().iParity = EParityNone;
		cfg().iHandshake = 0; cfg().iTerminatorCount = 0;
		at.SetConfig(cfg);
		TBuf8<128> rep;
		for (TInt k = 0; k < 3; ++k)
			if (At(at, "AT", rep) == KErrNone)
				break;
		if (At(at, "AT+QGPS?", rep) == KErrNone)
			{
			state = rep.Find(_L8("+QGPS: 1")) >= 0 ? 1 : 0;
			if (state == 1)
				{
				At(at, "AT+QGPSEND", rep);
				if (At(at, "AT+QGPS?", rep) == KErrNone && rep.Find(_L8("+QGPS: 0")) >= 0)
					state = 2;				// was on, now off
				}
			}
		at.Close();
		}
	srv.Close();
	return state;
	}

static TInt RunTest()
	{
	RDebug::Printf("PH: lbstest start");
	RPositionServer srv;
	TInt r = srv.Connect();
	RDebug::Printf("PH: lbstest RPositionServer::Connect -> %d", r);
	if (r != KErrNone)
		return r;

	TUint n = 0;
	r = srv.GetNumModules(n);
	RDebug::Printf("PH: lbstest GetNumModules -> %d, %d module(s)", r, (TInt)n);
	TBool found = EFalse;
	for (TUint i = 0; i < n; ++i)
		{
		TPositionModuleInfo info;
		if (srv.GetModuleInfoByIndex(i, info) != KErrNone)
			continue;
		TBuf<KPositionMaxModuleName> name;
		info.GetModuleName(name);
		RDebug::Printf("PH: lbstest module %d: id 0x%08x '%S' available %d", (TInt)i, info.ModuleId().iUid, &name, (TInt)info.IsAvailable());
		if (info.ModuleId() == KPheModule)
			found = ETrue;
		}
	RDebug::Printf("PH: lbstest the serial NMEA module %s", found ? "is listed" : "is NOT listed");

	TInt gotUpdate = 0, partials = 0;
	if (found)
		{
		RPositioner pos;
		r = pos.Open(srv, KPheModule);
		RDebug::Printf("PH: lbstest RPositioner::Open(module) -> %d", r);
		if (r == KErrNone)
			{
			pos.SetRequestor(CRequestor::ERequestorService, CRequestor::EFormatApplication, _L("phelbstest"));
			TPositionUpdateOptions opts;
			opts.SetUpdateInterval(TTimeIntervalMicroSeconds(0));
			opts.SetUpdateTimeOut(TTimeIntervalMicroSeconds(25000000));
			opts.SetMaxUpdateAge(TTimeIntervalMicroSeconds(0));
			opts.SetAcceptPartialUpdates(ETrue);
			r = pos.SetUpdateOptions(opts);
			RDebug::Printf("PH: lbstest SetUpdateOptions -> %d", r);
			for (TInt k = 0; k < KRounds && gotUpdate < 2; ++k)
				{
				TPositionSatelliteInfo sat;
				TRequestStatus rs;
				TTime t0; t0.UniversalTime();
				pos.NotifyPositionUpdate(sat, rs);
				User::WaitForRequest(rs);
				const TInt code = rs.Int();
				// the module status: device + data quality (no position involved)
				TPositionModuleStatus st;
				const TInt sr = srv.GetModuleStatus(st, KPheModule);
				RDebug::Printf("PH: lbstest update %d after %d ms: status %d; module status %d: device %d quality %d",
							   k, Ms(t0), code, sr, (TInt)st.DeviceStatus(), (TInt)st.DataQualityStatus());
				if (code == KPositionPartialUpdate || code == KErrNone || code == KPositionQualityLoss)
					{
					++gotUpdate;
					if (code == KPositionPartialUpdate)
						++partials;
					RDebug::Printf("PH: lbstest satellites in view %d, used %d (class 0x%x)",
								   (TInt)sat.NumSatellitesInView(), (TInt)sat.NumSatellitesUsed(), (TInt)sat.PositionClassType());
					}
				}
			pos.Close();
			}
		}
	srv.Close();
	User::After(3000000);			// the server drops the module after its last session: GNSS is switched off then
	const TInt g = EnsureGnssOff();
	RDebug::Printf("PH: lbstest GNSS after the test: %s", g == 0 ? "off (the PSY switched it off)" : g == 2 ? "was still on, switched off here" : "state unknown");
	const TBool pass = found && gotUpdate > 0 && g >= 0;
	RDebug::Printf("PH: lbstest done (%s): %d update(s), %d partial", pass ? "PASS" : "FAIL", gotUpdate, partials);
	return pass ? KErrNone : KErrGeneral;
	}

// the location API leaves and uses the cleanup stack (the earlier tests used none): install one, as every Symbian EXE does
GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	const TInt r = RunTest();
	delete cleanup;
	return r;
	}
