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
	TRequestStatus st;
	RDebug::Printf("[phedata] RConnection::Start(iap %u)...", iap);
	conn.Start(pref, st);
	TInt r = Await(st, "RConnection::Start");
	if (r != KErrNone)
		{
		if (r == KErrTimedOut) conn.Stop();
		User::Leave(r);
		}
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
