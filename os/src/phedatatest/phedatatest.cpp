// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phedatatest -- the mobile data test: the comms stack over the modem's USB Ethernet interface (Ethernet.USB PDD,
// USBHOST_NET=1), the CommsDat "Ethernet - DHCP" IAP (cmsetup's / CED's), RConnection::Start (EtherPkt -> ethint -> the
// PDD, DHCP from the modem), then DNS + an HTTP GET. [phedata] lines only; the APN is the modem's own configuration
// (set with AT+CGDCONT before the test, never in this file). Every wait is bounded.
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <in_sock.h>
#include <commdb.h>
#include <cdbcols.h>
#include <commdbconnpref.h>
#include <f32file.h>
#include <e32property.h>
#include <cmmanager.h>
#include <cmgenconnsettings.h>

_LIT(KIapName, "Ethernet - DHCP");
static const TInt KStepTimeout = 90 * 1000000;
static const TInt KHoldSeconds = 30;

static TInt Await(TRequestStatus& aStatus, const char* aWhat)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, KStepTimeout);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("[phedata] %s: TIMED OUT", aWhat);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	RDebug::Printf("[phedata] %s: %d", aWhat, aStatus.Int());
	return aStatus.Int();
	}

static TUint32 FindIapL()
	{
	CCommsDatabase* db = CCommsDatabase::NewL();
	CleanupStack::PushL(db);
	CCommsDbTableView* v = db->OpenViewMatchingTextLC(TPtrC(IAP), TPtrC(COMMDB_NAME), KIapName);
	TUint32 id = 0;
	if (v->GotoFirstRecord() == KErrNone)
		v->ReadUintL(TPtrC(COMMDB_ID), id);
	CleanupStack::PopAndDestroy(2);
	return id;
	}

// what the Ethernet NIF will do first (ethint -> EtherPkt, CardCtl.cpp LoadDeviceDriversL): the LDD/PDD file and device names it
// reads from the LANBearer table, and the ini file it needs; printed so a failure before the driver is opened names its cause
static void ReadBearerColumn(CCommsDbTableView& aV, const TDesC& aCol, const char* aLabel)
	{
	TBuf<KCommsDbSvrMaxFieldLength> v;
	TRAPD(e, aV.ReadTextL(aCol, v));
	TBuf8<KCommsDbSvrMaxFieldLength> v8;
	v8.Copy(v);
	if (e == KErrNone)
		RDebug::Printf("[phedata] LANBearer %s = \"%S\"", aLabel, &v8);
	else
		RDebug::Printf("[phedata] LANBearer %s: read error %d", aLabel, e);
	}

static void DiagnoseL()
	{
	TInt autolock = -99, saver = -99;
	const TInt ae = RProperty::Get(TUid::Uid(0x101F8767), 0x501, autolock);	// KCoreAppUIsAutolockStatus (1 = off)
	const TInt se = RProperty::Get(TUid::Uid(0x101F8771), 0x3, saver);		// KScreenSaverOn
	RDebug::Printf("[phedata] lock state: autolock %d (err %d), screensaver on %d (err %d); the connection dialogs cancel while locked", autolock, ae, saver, se);
	CCommsDatabase* db = CCommsDatabase::NewL();
	CleanupStack::PushL(db);
	CCommsDbTableView* v = db->OpenTableLC(TPtrC(LAN_BEARER));
	TInt n = 0;
	for (TInt e = v->GotoFirstRecord(); e == KErrNone && n < 4; e = v->GotoNextRecord(), ++n)
		{
		RDebug::Printf("[phedata] LANBearer record %d", n);
		ReadBearerColumn(*v, TPtrC(COMMDB_NAME), "Name");
		ReadBearerColumn(*v, TPtrC(LAN_BEARER_LDD_FILENAME), "LDDFilename");
		ReadBearerColumn(*v, TPtrC(LAN_BEARER_LDD_NAME), "LDDName");
		ReadBearerColumn(*v, TPtrC(LAN_BEARER_PDD_FILENAME), "PDDFilename");
		ReadBearerColumn(*v, TPtrC(LAN_BEARER_PDD_NAME), "PDDName");
		ReadBearerColumn(*v, TPtrC(LAN_BEARER_PACKET_DRIVER_NAME), "PacketDriverName");
		}
	if (n == 0)
		RDebug::Printf("[phedata] LANBearer table: no record");
	CleanupStack::PopAndDestroy(2);		// view, db
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		TEntry en;
		const TInt ec = fs.Entry(_L("c:\\private\\101f7989\\esock\\ether802.ini"), en);
		const TInt ez = fs.Entry(_L("z:\\private\\101f7989\\esock\\ether802.ini"), en);
		RDebug::Printf("[phedata] ether802.ini: c: %d, z: %d (0 = present)", ec, ez);
		static const TText* const KFiles[] = { _S("z:\\sys\\bin\\ethint.nif"), _S("z:\\sys\\bin\\etherpkt.drv"), _S("z:\\sys\\bin\\enet.ldd"), _S("z:\\sys\\bin\\ethernet.pdd") };
		for (TInt i = 0; i < 4; ++i)
			{
			TBuf<64> f(TPtrC(KFiles[i]));
			TBuf8<64> f8; f8.Copy(f);
			RDebug::Printf("[phedata] file %S: %d (0 = present; enet.ldd and ethernet.pdd are expected absent: the kernel has them built in)", &f8, fs.Entry(f, en));
			}
		fs.Close();
		}
	}

static void FetchL(RSocketServ& aSs, RConnection& aConn)
	{
	_LIT(KHost, "example.com");
	TRequestStatus st;
	RHostResolver hr;
	User::LeaveIfError(hr.Open(aSs, KAfInet, KProtocolInetUdp, aConn));
	CleanupClosePushL(hr);
	TNameEntry ne;
	hr.GetByName(KHost, ne, st);
	TInt r = Await(st, "RHostResolver::GetByName");
	if (r != KErrNone)
		{
		if (r == KErrTimedOut) hr.Cancel();
		User::Leave(r);
		}
	TInetAddr addr(ne().iAddr);
	TBuf<64> a16; addr.Output(a16);
	TBuf8<64> a8; a8.Copy(a16);
	RDebug::Printf("[phedata] example.com is %S", &a8);
	RSocket sock;
	User::LeaveIfError(sock.Open(aSs, KAfInet, KSockStream, KProtocolInetTcp, aConn));
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
	RDebug::Printf("[phedata] local address %S", &l8);
	TBuf8<128> req;
	req.Copy(_L8("GET / HTTP/1.0\r\nHost: example.com\r\nUser-Agent: Phorepoc-phedata\r\n\r\n"));
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
	TInt nl = resp.Locate('\n');
	TPtrC8 line = nl < 0 ? TPtrC8(resp) : resp.Left(nl);
	RDebug::Printf("[phedata] %d bytes: | %S", resp.Length(), &line);
	CleanupStack::PopAndDestroy(2);		// sock, hr
	}

static void RunL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CActiveScheduler::Install(sched);
	CleanupStack::PushL(sched);
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
	RDebug::Printf("[phedata] cmsetup (the S60 connection tables, MPM's input): %d", cr);
	}
	TRAPD(de, DiagnoseL());
	RDebug::Printf("[phedata] diagnostics: %d", de);
	TUint32 iap = FindIapL();
	RDebug::Printf("[phedata] IAP \"Ethernet - DHCP\" = %u", iap);
	if (!iap)
		User::Leave(KErrNotFound);
	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);
	RConnection conn;
	User::LeaveIfError(conn.Open(ss));
	CleanupClosePushL(conn);
	TCommDbConnPref pref;
	pref.SetIapId(iap);
	pref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
	pref.SetDirection(ECommDbConnectionDirectionOutgoing);
	pref.SetBearerSet(ECommDbBearerLAN);
	// MPM asks "use mobile data abroad?" before a non-WLAN IAP when the stored cellular data usage is Confirm (the CommsDat
	// DefConn default for a visitor network, cmmcache.cpp; the query is CMPMConfirmDlgRoaming). Unanswered (or hidden behind
	// another note) it cancels the start. The test sets Automatic for the run and puts the user's values back after Start.
	TCmGenConnSettings gen, saved;
	RCmManager cm;
	TRAPD(ce, { cm.OpenL(); cm.ReadGenConnSettingsL(saved); gen = saved;
	            gen.iCellularDataUsageHome = ECmCellularDataUsageAutomatic; gen.iCellularDataUsageVisitor = ECmCellularDataUsageAutomatic;
	            cm.WriteGenConnSettingsL(gen); });
	RDebug::Printf("[phedata] cellular data usage home %d visitor %d -> automatic for the test: %d", saved.iCellularDataUsageHome,
		saved.iCellularDataUsageVisitor, ce);
	TRequestStatus st;
	RDebug::Printf("[phedata] RConnection::Start(iap %u)...", iap);
	conn.Start(pref, st);
	// every progress notification (stage + error) until Start completes: a KErrCancel at the end is only a verdict, the
	// last stage and its error name the step that failed (KConfigDaemonLoading 3500.., KLinkLayerOpen 7000, ...)
	TNifProgressBuf pb;
	TRequestStatus pst, ts;
	RTimer tm;
	tm.CreateLocal();
	tm.After(ts, 1000000);
	TInt ticks = 0;
	User::ResetInactivityTime();
	conn.ProgressNotification(pb, pst);
	TBool stOut = ETrue, pstOut = ETrue, tsOut = ETrue;
	TInt nProg = 0;
	while (stOut)
		{
		User::WaitForAnyRequest();
		if (pstOut && pst != KRequestPending)
			{
			pstOut = EFalse;
			if (nProg++ < 40)
				RDebug::Printf("[phedata] progress: stage %d error %d (status %d)", pb().iStage, pb().iError, pst.Int());
			if (stOut && pst == KErrNone)
				{
				conn.ProgressNotification(pb, pst);
				pstOut = ETrue;
				}
			}
		if (stOut && st != KRequestPending)
			stOut = EFalse;
		else if (tsOut && ts != KRequestPending)
			{
			User::ResetInactivityTime();		// the display and the lock timers stay as they were: only the idle counter restarts
			if (++ticks >= KStepTimeout / 1000000)
				{
				tsOut = EFalse;
				RDebug::Printf("[phedata] RConnection::Start: TIMED OUT");
				conn.Stop();
				}
			else
				tm.After(ts, 1000000);
			}
		}
	if (pstOut)
		{
		conn.CancelProgressNotification();
		User::WaitForRequest(pst);
		}
	if (tsOut)
		{
		tm.Cancel();
		User::WaitForRequest(ts);
		}
	tm.Close();
	TNifProgress last;
	conn.Progress(last);
	RDebug::Printf("[phedata] RConnection::Start: %d (last progress: stage %d error %d)", st.Int(), last.iStage, last.iError);
	if (ce == KErrNone)
		{
		TRAPD(re, cm.WriteGenConnSettingsL(saved));
		RDebug::Printf("[phedata] cellular data usage restored: %d", re);
		}
	cm.Close();
	TInt r = st.Int();
	if (r != KErrNone)
		User::Leave(r);
	TRAPD(fe, FetchL(ss, conn));
	RDebug::Printf("[phedata] fetch: %d", fe);
	User::After(KHoldSeconds * 1000000);
	conn.Close();
	RDebug::Printf("[phedata] %s", fe == KErrNone ? "PASS" : "connected, fetch failed");
	CleanupStack::PopAndDestroy(3);		// conn, ss, sched
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	RDebug::Printf("[phedata] start");
	TRAPD(err, RunL());
	RDebug::Printf("[phedata] done err=%d", err);
	delete tc;
	return err;
	}
