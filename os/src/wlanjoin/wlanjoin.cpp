// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// wlanjoin.cpp -- OURS (2026-10-10, PinePhone WiFi W4): does the 2010 stack JOIN a network through our WHA PDD?
//   1. the WLAN IAP in CommsDat, the way the S60 WLAN connection-method plug-in (cmpluginwlan) and the WPA settings
//      library write it: a LANService record (DHCP via NetCfgExtnDhcp), a WLANServiceTable record (ServiceID -> the
//      LANService, SSID, connection mode, security mode, the pre-shared key), an IAP record on dbcreator's
//      "WLANBearer" (wlanagt.agt + wlannif) -- named "Phorepoc WLAN", reused when it exists;
//   2. RConnection::Start on that IAP: wlanagt -> wlmserver (Join -> the UMAC's authentication + association ->
//      EAPOL for WPA) -> wlannif up -> DHCP;
//   3. DNS + an HTTP GET as nettest does, the local address logged;
//   4. the connection kept 60 s (traffic + the engine's roaming/bss-lost logic), then stopped.
// The network: C:\wlanjoin.txt = "<ssid>|<psk>" (psk empty = open), else the open "EE WiFi" seen in W3's scans.
// [wlanjoin] lines (A64_RDEBUG_LOG). Every wait is bounded.
#include <e32base.h>
#include <e32debug.h>
#include <e32property.h>
#include <es_sock.h>
#include <in_sock.h>
#include <f32file.h>
#include <commdb.h>
#include <cdbcols.h>
#include <commdbconnpref.h>
#include <wlancdbcols.h>
#include <btengsettings.h>
#include <btengconstants.h>

_LIT(KIapName, "Phorepoc WLAN");
_LIT(KStoredIapName, "ASUS");
_LIT(KWlanBearerName, "WLANBearer");
_LIT(KLanServiceType, "LANService");
_LIT(KLanBearerType, "LANBearer");
_LIT(KConfigFile, "c:\\wlanjoin.txt");
_LIT(KDefaultSsid, "Test");		// the user's open guest SSID on the ASUS (2026-10-10 evening, up for two hours)
_LIT(KDefaultPsk, "");
static const TInt KStepTimeout = 150 * 1000000;	// the engine's connect = scan + 5 join tries + DHCP; 60 s cut boot 257 short
static const TInt KHoldSeconds = 60;
enum { EWlanSecModeOpen = 1, EWlanSecModeWpa = 8, EWlanSecModeWpa2 = 16, EWlanConnModeInfra = 1 };

static TInt Await(TRequestStatus& aStatus, const char* aWhat)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, KStepTimeout);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("[wlanjoin] %s: TIMED OUT after %d s", aWhat, KStepTimeout / 1000000);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	RDebug::Printf("[wlanjoin] %s: %d", aWhat, aStatus.Int());
	return aStatus.Int();
	}

// "<ssid>|<psk>" from C:\wlanjoin.txt, or the default open network
static void ReadConfig(TDes& aSsid, TDes& aPsk)
	{
	aSsid = KDefaultSsid;
	aPsk = KDefaultPsk;
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	RFile f;
	if (f.Open(fs, KConfigFile, EFileRead | EFileShareReadersOnly) == KErrNone)
		{
		TBuf8<128> b;
		f.Read(b);
		f.Close();
		while (b.Length() && (b[b.Length() - 1] == '\r' || b[b.Length() - 1] == '\n' || b[b.Length() - 1] == ' '))
			b.SetLength(b.Length() - 1);
		TInt bar = b.Locate('|');
		if (bar < 0)
			aSsid.Copy(b.Left(32));
		else
			{
			aSsid.Copy(b.Left(bar > 32 ? 32 : bar));
			aPsk.Copy(b.Mid(bar + 1).Left(63));
			}
		}
	fs.Close();
	}

static TUint32 FindRecordByNameL(CCommsDatabase& aDb, const TDesC& aTable, const TDesC& aName)
	{
	CCommsDbTableView* v = aDb.OpenViewMatchingTextLC(aTable, TPtrC(COMMDB_NAME), aName);
	TUint32 id = 0;
	if (v->GotoFirstRecord() == KErrNone)
		v->ReadUintL(TPtrC(COMMDB_ID), id);
	CleanupStack::PopAndDestroy(v);
	return id;
	}

// the IAP id of "Phorepoc WLAN", created on first use
static TUint32 EnsureIapL(const TDesC& aSsid, const TDesC& aPsk)
	{
	CCommsDatabase* db = CCommsDatabase::NewL();
	CleanupStack::PushL(db);
	TUint32 iap = FindRecordByNameL(*db, TPtrC(IAP), KIapName);
	TUint32 bearer = FindRecordByNameL(*db, TPtrC(LAN_BEARER), KWlanBearerName);
	RDebug::Printf("[wlanjoin] CommsDat: existing IAP %u, WLANBearer %u", iap, bearer);
	if (!bearer)
		{
		// dbcreator's record (cdcwlantables.cpp CreateWlanBearerRecordL) when the shipped database lacks it
		CCommsDbTableView* v = db->OpenTableLC(TPtrC(LAN_BEARER));
		User::LeaveIfError(v->InsertRecord(bearer));
		v->WriteTextL(TPtrC(COMMDB_NAME), KWlanBearerName);
		v->WriteTextL(TPtrC(LAN_BEARER_AGENT), _L("wlanagt.agt"));
		v->WriteTextL(TPtrC(LAN_BEARER_NIF_NAME), _L("wlannif"));
		v->WriteTextL(TPtrC(LAN_BEARER_LDD_FILENAME), _L("wlanldd"));
		v->WriteTextL(TPtrC(LAN_BEARER_LDD_NAME), _L("wlan"));
		v->WriteTextL(TPtrC(LAN_BEARER_PDD_FILENAME), _L("wlanpdd"));
		v->WriteTextL(TPtrC(LAN_BEARER_PDD_NAME), _L("wlan.phys"));
		v->WriteUintL(TPtrC(LAST_SOCKET_ACTIVITY_TIMEOUT), 180);
		v->WriteUintL(TPtrC(LAST_SESSION_CLOSED_TIMEOUT), 30);
		v->WriteUintL(TPtrC(LAST_SOCKET_CLOSED_TIMEOUT), 180);
		User::LeaveIfError(v->PutRecordChanges());
		CleanupStack::PopAndDestroy(v);
		RDebug::Printf("[wlanjoin] CommsDat: WLANBearer created, id %u", bearer);
		}
	TUint32 service = 0;
	if (iap)
		{
		CCommsDbTableView* v = db->OpenViewMatchingUintLC(TPtrC(IAP), TPtrC(COMMDB_ID), iap);
		if (v->GotoFirstRecord() == KErrNone)
			v->ReadUintL(TPtrC(IAP_SERVICE), service);
		CleanupStack::PopAndDestroy(v);
		}
	else
		{
		CCommsDbTableView* v = db->OpenTableLC(TPtrC(LAN_SERVICE));
		User::LeaveIfError(v->InsertRecord(service));
		v->WriteTextL(TPtrC(COMMDB_NAME), KIapName);
		v->WriteTextL(TPtrC(LAN_IF_NETWORKS), _L("ip"));
		v->WriteBoolL(TPtrC(LAN_IP_ADDR_FROM_SERVER), ETrue);
		v->WriteBoolL(TPtrC(LAN_IP_DNS_ADDR_FROM_SERVER), ETrue);
		v->WriteBoolL(TPtrC(LAN_IP6_DNS_ADDR_FROM_SERVER), ETrue);
		v->WriteTextL(TPtrC(LAN_CONFIG_DAEMON_MANAGER_NAME), _L("NetCfgExtnDhcp"));
		v->WriteTextL(TPtrC(LAN_CONFIG_DAEMON_NAME), _L("!DhcpServ"));
		User::LeaveIfError(v->PutRecordChanges());
		CleanupStack::PopAndDestroy(v);
		v = db->OpenTableLC(TPtrC(IAP));
		User::LeaveIfError(v->InsertRecord(iap));
		v->WriteTextL(TPtrC(COMMDB_NAME), KIapName);
		v->WriteUintL(TPtrC(IAP_SERVICE), service);
		v->WriteTextL(TPtrC(IAP_SERVICE_TYPE), KLanServiceType);
		v->WriteUintL(TPtrC(IAP_BEARER), bearer);
		v->WriteTextL(TPtrC(IAP_BEARER_TYPE), KLanBearerType);
		v->WriteUintL(TPtrC(IAP_NETWORK), 1);
		v->WriteUintL(TPtrC(IAP_NETWORK_WEIGHTING), 0);
		v->WriteUintL(TPtrC(IAP_LOCATION), 1);
		User::LeaveIfError(v->PutRecordChanges());
		CleanupStack::PopAndDestroy(v);
		RDebug::Printf("[wlanjoin] CommsDat: LANService %u + IAP %u created", service, iap);
		}
	// the WLAN record for the service: always (re)written with the configured network
	CCommsDbTableView* w = db->OpenViewMatchingUintLC(TPtrC(WLAN_SERVICE), TPtrC(WLAN_SERVICE_ID), service);
	TUint32 wid = 0;
	if (w->GotoFirstRecord() == KErrNone)
		User::LeaveIfError(w->UpdateRecord());
	else
		{
		User::LeaveIfError(w->InsertRecord(wid));
		w->WriteUintL(TPtrC(WLAN_SERVICE_ID), service);
		}
	const TBool psk = aPsk.Length() > 0;
	RDebug::Printf("[wlanjoin] CommsDat: WLAN record %u for service %u (%s)", wid, service, wid ? "new" : "update");
	w->WriteTextL(TPtrC(COMMDB_NAME), KIapName);
	w->WriteUintL(TPtrC(WLAN_CONNECTION_MODE), EWlanConnModeInfra);
	w->WriteTextL(TPtrC(NU_WLAN_SSID), aSsid);		// "WlanSSID" (cdbcols.h's WLAN_SSID = "Wlan_SSID" is not this table's column: -1)
	w->WriteUintL(TPtrC(WLAN_SECURITY_MODE), psk ? EWlanSecModeWpa2 : EWlanSecModeOpen);
	w->WriteUintL(TPtrC(WLAN_ENABLE_WPA_PSK), psk ? 1 : 0);
	TBuf8<64> psk8;
	psk8.Copy(aPsk);
	w->WriteTextL(TPtrC(WLAN_WPA_PRE_SHARED_KEY), psk8);
	w->WriteUintL(TPtrC(WLAN_WPA_KEY_LENGTH), psk8.Length());
	w->WriteBoolL(TPtrC(WLAN_SCAN_SSID), EFalse);
	w->WriteUintL(TPtrC(NU_WLAN_CHANNEL_ID), 0);
	RDebug::Printf("[wlanjoin] CommsDat: WLAN record columns written");
	User::LeaveIfError(w->PutRecordChanges());
	CleanupStack::PopAndDestroy(w);
	TBuf8<40> s8;
	s8.Copy(aSsid);
	RDebug::Printf("[wlanjoin] CommsDat: WLAN record for service %u: ssid \"%S\" %s", service, &s8, psk ? "WPA2-PSK" : "open");
	CleanupStack::PopAndDestroy(db);
	return iap;
	}

static void FetchL(RSocketServ& aSs, RConnection& aConn)
	{
	_LIT(KHost, "example.com");
	TRequestStatus st;
	RHostResolver hr;
	TInt r = hr.Open(aSs, KAfInet, KProtocolInetUdp, aConn);
	RDebug::Printf("[wlanjoin] RHostResolver::Open: %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(hr);
	TNameEntry ne;
	hr.GetByName(KHost, ne, st);
	if ((r = Await(st, "RHostResolver::GetByName")) != KErrNone)
		{
		if (r == KErrTimedOut) hr.Cancel();
		User::Leave(r);
		}
	TInetAddr addr(ne().iAddr);
	TBuf<64> a16; addr.Output(a16);
	TBuf8<64> a8; a8.Copy(a16);
	RDebug::Printf("[wlanjoin] example.com is %S", &a8);
	RSocket sock;
	r = sock.Open(aSs, KAfInet, KSockStream, KProtocolInetTcp, aConn);
	User::LeaveIfError(r);
	CleanupClosePushL(sock);
	addr.SetPort(80);
	sock.Connect(addr, st);
	if ((r = Await(st, "RSocket::Connect")) != KErrNone)
		{
		if (r == KErrTimedOut) sock.CancelConnect();
		User::Leave(r);
		}
	TInetAddr local; sock.LocalName(local);
	TBuf<64> l16; local.Output(l16);
	TBuf8<64> l8; l8.Copy(l16);
	RDebug::Printf("[wlanjoin] local address %S", &l8);
	TBuf8<256> req;
	req.Copy(_L8("GET / HTTP/1.0\r\nHost: example.com\r\nUser-Agent: Phorepoc-wlanjoin\r\n\r\n"));
	sock.Write(req, st);
	User::LeaveIfError(Await(st, "RSocket::Write"));
	TBuf8<512> resp;
	TSockXfrLength len;
	sock.RecvOneOrMore(resp, 0, st, len);
	if ((r = Await(st, "RSocket::RecvOneOrMore")) != KErrNone)
		{
		if (r == KErrTimedOut) sock.CancelRecv();
		User::Leave(r);
		}
	RDebug::Printf("[wlanjoin] %d bytes received:", resp.Length());
	TPtrC8 rest(resp);
	for (TInt n = 0; n < 4 && rest.Length(); ++n)
		{
		TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		if (line.Length() && line[line.Length() - 1] == '\r')
			line.Set(line.Left(line.Length() - 1));
		RDebug::Printf("[wlanjoin] | %S", &line);
		rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
		}
	CleanupStack::PopAndDestroy(2);		// sock, hr
	}

static TBool PhwebtestRan;
// the WebKit page test (phwebtest.exe, the ARM browser; "phwebtest: ..." lines), when the ROM carries it: over the joined
// connection, or with no network (`--offline`: straight to its built-in page) when the join failed -- a fetch with no
// connection never ends (curl's threaded resolver blocks in the implicit RConnection start, curl_easy_cleanup joins it)
static void RunPhwebtest(TBool aOffline)
	{
	if (PhwebtestRan)
		return;
	PhwebtestRan = ETrue;
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		fs.MkDirAll(_L("C:\\data\\"));
		RFile f;
		if (f.Replace(fs, _L("C:\\data\\phweb_iap.txt"), EFileWrite) == KErrNone)
			{
			f.Write(_L8("ASUS\n"));
			f.Close();
			}
		fs.Close();
		}
	RProcess p;
	TInt pr = p.Create(_L("phwebtest.exe"), aOffline ? TPtrC(_L("--offline")) : TPtrC());
	RDebug::Printf("[wlanjoin] phwebtest.exe: create %d", pr);
	if (pr == KErrNone)
		{
		TRequestStatus ps;
		p.Logon(ps);
		p.Resume();
		RTimer t;
		t.CreateLocal();
		TRequestStatus ts;
		t.After(ts, 600000000);
		User::WaitForRequest(ps, ts);
		if (ps == KRequestPending) { p.Kill(KErrTimedOut); User::WaitForRequest(ps); }
		else { t.Cancel(); User::WaitForRequest(ts); }
		RDebug::Printf("[wlanjoin] phwebtest.exe: exit %d (type %d)", ps.Int(), p.ExitType());
		t.Close();
		p.Close();
		}
	}

static void RunL()
	{
	// Bluetooth on first, the state a Linux PinePhone runs WiFi in (Mobian: hci_h5 + btrtl load the BT patch at boot,
	// bluetoothd powers it): the chip's WiFi coex hands the shared radio to the hardware arbiter (PTA) between the two
	// halves. With the BT half held in reset our MAC's ACKs never reached the AP (boots 257-269); with it out of
	// reset but unpatched nothing reached the air (boot 266) -- the test: does WiFi ACK with a running BT half? The S60 way, as btengon (R4): CBTEngSettings ->
	// btengsrv -> RBTPowerControl -> H5, the patch download, the stack's init. The session is held for the whole run
	// (the engine stops the stack 3 s after its last client).
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	CBTEngSettings* bt = CBTEngSettings::NewLC();
#ifdef WLANJOIN_BT_ON
	TInt br = bt->SetPowerState(EBTPowerOn);
	RDebug::Printf("[wlanjoin] Bluetooth SetPowerState(on) -> %d", br);
	User::After(10000000);			// the patch download + the stack's init (~3 s)
	TBTPowerStateValue power = EBTPowerOff;
	br = bt->GetPowerState(power);
	RDebug::Printf("[wlanjoin] Bluetooth GetPowerState -> %d, state %d (1 = on)", br, power);
#else
	RDebug::Printf("[wlanjoin] Bluetooth left off (WLANJOIN_BT_ON not set)");
#endif
	// the stored "ASUS" access point (WPA2, its key already in CommsDat; user 2026-10-14). "Phorepoc WLAN" (EnsureIapL, the
	// expired open "Test" SSID) is no longer created: without ASUS the run stops here
	TUint32 iap = 0;
	{
	CCommsDatabase* db = CCommsDatabase::NewL();
	CleanupStack::PushL(db);
	iap = FindRecordByNameL(*db, TPtrC(IAP), KStoredIapName);
	CleanupStack::PopAndDestroy(db);
	}
	RDebug::Printf("[wlanjoin] CommsDat: IAP \"ASUS\" = %u", iap);
	if (!iap)
		User::Leave(KErrNotFound);
	// the WLAN IAP exists: cmsetup puts it into the "Internet" destination and drops the LAN (Ethernet) members the ROM's
	// x86-built CommsDat carries (enet.ldd is absent: every MPM connect otherwise pops "Connection failed" / "unavailable").
	// Here, not only in phebrlaunch: this is the default SSM test, the browser is opened from the app grid (boot 878).
		{
		RProcess cms;
		TInt cr = cms.Create(_L("cmsetup.exe"), KNullDesC);
		if (cr == KErrNone)
			{
			TRequestStatus cs;
			cms.Logon(cs);
			cms.Resume();
			User::WaitForRequest(cs);
			cr = cs.Int();
			cms.Close();
			}
		RDebug::Printf("PH: [wlanjoin] cmsetup: %d", cr);
		}
	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);
	RConnection conn;
	User::LeaveIfError(conn.Open(ss));
	CleanupClosePushL(conn);
	TCommDbConnPref pref;
	pref.SetIapId(iap);
	pref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
	TRequestStatus st;
	RDebug::Printf("[wlanjoin] RConnection::Start(iap %u)...", iap);
	conn.Start(pref, st);
	TInt r = Await(st, "RConnection::Start");
	if (r != KErrNone)
		{
		if (r == KErrTimedOut) conn.Stop();
		User::Leave(r);
		}
	TRAPD(fe, FetchL(ss, conn));
	RDebug::Printf("[wlanjoin] fetch: %d", fe);
	// network time (the 2010 tree has none: phones took NITZ from the cellular network): ntpsync (X4.50, OURS) as a
	// daemon on this IAP, every 30 min; give it a few seconds before the TLS test (certificate dates)
	{
	RProcess p;
	TBuf<48> args;
	args.Format(_L("pool.ntp.org 1800 %u"), iap);
	TInt pr = p.Create(_L("ntpsync.exe"), args);
	RDebug::Printf("[wlanjoin] ntpsync.exe: create %d", pr);
	if (pr == KErrNone)
		{
		p.Resume();
		p.Close();
		User::After(8000000);
		}
	}
	// TLS over the same connection: ssltest.exe (OURS, the mbedTLS libssl.dll drop-in; "[ssltest] RESULT ...") while
	// the connection is held, when the ROM carries it
	{
	RProcess p;
	TInt pr = p.Create(_L("ssltest.exe"), _L("example.com 443 ASUS"));
	RDebug::Printf("[wlanjoin] ssltest.exe: create %d", pr);
	if (pr == KErrNone)
		{
		TRequestStatus ps;
		p.Logon(ps);
		p.Resume();
		RTimer t;
		t.CreateLocal();
		TRequestStatus ts;
		t.After(ts, 60000000);
		User::WaitForRequest(ps, ts);
		if (ps == KRequestPending) { p.Kill(KErrTimedOut); User::WaitForRequest(ps); }
		else { t.Cancel(); User::WaitForRequest(ts); }
		RDebug::Printf("[wlanjoin] ssltest.exe: exit %d", ps.Int());
		t.Close();
		p.Close();
		}
	}
	// on-device TLS tests while the connection is held: C:\data\sslresume_args.txt = one argument line per run of
	// sslresume.exe (the tls repo's session-resumption test: "host port"), absent = none. 2026-10-21
	{
	RFs tfs;
	RFile tf;
	TBuf8<400> lines;
	if (tfs.Connect() == KErrNone)
		{
		if (tf.Open(tfs, _L("C:\\data\\sslresume_args.txt"), EFileRead | EFileShareReadersOnly) == KErrNone)
			{
			tf.Read(lines);
			tf.Close();
			}
		tfs.Close();
		}
	TInt pos = 0;
	while (pos < lines.Length())
		{
		TInt eol = lines.Mid(pos).Locate('\n');
		TPtrC8 l8 = eol < 0 ? lines.Mid(pos) : lines.Mid(pos, eol);
		pos += (eol < 0 ? lines.Length() - pos : eol + 1);
		TBuf<80> args;
		args.Copy(l8.Left(80));
		args.Trim();
		if (!args.Length())
			continue;
		RProcess p;
		TInt pr = p.Create(_L("sslresume.exe"), args);
		RDebug::Printf("[wlanjoin] sslresume.exe %S: create %d", &args, pr);
		if (pr != KErrNone)
			continue;
		TRequestStatus ps;
		p.Logon(ps);
		p.Resume();
		RTimer t;
		t.CreateLocal();
		TRequestStatus ts;
		t.After(ts, 90000000);
		User::WaitForRequest(ps, ts);
		if (ps == KRequestPending) { p.Kill(KErrTimedOut); User::WaitForRequest(ps); }
		else { t.Cancel(); User::WaitForRequest(ts); }
		RDebug::Printf("[wlanjoin] sslresume.exe: exit %d", ps.Int());
		t.Close();
		p.Close();
		}
	}
	RunPhwebtest(EFalse);
	RDebug::Printf("[wlanjoin] holding the connection %d s", KHoldSeconds);
	User::After(KHoldSeconds * 1000000);
	conn.Stop();
	RDebug::Printf("[wlanjoin] stopped; %s", fe == KErrNone ? "PASS" : "joined, fetch failed");
	CleanupStack::PopAndDestroy(4);		// conn, ss, bt, sched
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	RDebug::Printf("[wlanjoin] start");
	TRAPD(err, RunL());
	RDebug::Printf("[wlanjoin] done err=%d", err);
	if (err != KErrNone)
		RunPhwebtest(ETrue);
	// the connection dialogs (WLAN key queries...) cancel themselves while either says "locked"
	// (ConnectionDialogsNotifBase.cpp ScreenSaverOn/AutolockOn; boot 314: autolock 0 = no lockapp)
	{
	TInt autolock = -99, saver = -99;
	const TInt ae = RProperty::Get(TUid::Uid(0x101F8767), 0x501, autolock);	// KCoreAppUIsAutolockStatus (1 = off)
	const TInt se = RProperty::Get(TUid::Uid(0x101F8771), 0x3, saver);		// KScreenSaverOn
	RDebug::Printf("[wlanjoin] lock state: autolock %d (err %d), screensaver on %d (err %d)", autolock, ae, saver, se);
	}
	delete tc;
	return err;
	}
