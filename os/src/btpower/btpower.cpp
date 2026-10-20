// btpower.cpp -- OURS (2026-10-10, Bluetooth rung R2): does the 2010 Bluetooth stack come up on the PinePhone's
// RTL8723CS over our H5 HCTL? RBTPowerControl::Open starts the stack (RSocketServ::StartProtocol of btlinkmanager
// inside c32exe's ESock_Bt worker -> bt_v2.prt -> corehci_symbian -> hctl_h5 + initialisor_symbian), the stack then
// runs its own HCI initialisation (HCI_Reset, Read_BD_ADDR, Read_Local_Version, ...) on the chip's ROM firmware.
// We ask the power state, power on if needed, then open a link-manager (ACL) socket and print its local name = the
// controller's BD_ADDR, and the local device name through a host resolver. Everything is logged (A64_RDEBUG_LOG);
// the HCTL's own [hctl_h5] lines show the link establishment and the first HCI frames. SSM_TEST=btpower stages it as
// ssmtest.exe, started by the SSM's non-critical list.
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <bt_sock.h>
#include <bluetooth/btpowercontrol.h>
#include <bluetooth/aclsockaddr.h>		// KSockBluetoothTypeACL

static TInt WaitFor(TRequestStatus& aStatus, TInt aUs)
	{
	TRequestStatus ts;
	RTimer t;
	t.CreateLocal();
	t.After(ts, aUs);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	return aStatus.Int();
	}

static void PrintAddr(const char* aTag, const TBTDevAddr& aAddr)
	{
	TBuf<20> s;
	aAddr.GetReadable(s, KNullDesC, _L(":"), KNullDesC);
	TBuf8<24> n;
	n.Copy(s);
	RDebug::Printf("[btpower] %s %S", aTag, &n);
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[btpower] start");
	CTrapCleanup* cleanup = CTrapCleanup::New();		// the client libraries leave and push (RHCIServerSession::OpenL)
	if (!cleanup)
		return KErrNoMemory;
	RBTPowerControl pc;
	TInt r = pc.Open();									// loads the stack if it is not running
	RDebug::Printf("[btpower] RBTPowerControl::Open -> %d", r);
	if (r != KErrNone)
		return r;
	TBTPowerState state = EBTOff;
	r = pc.GetPower(state, NULL);
	RDebug::Printf("[btpower] GetPower -> %d, state %d (0 = on)", r, state);
	if (r != KErrNone || state != EBTOn)
		{
		TRequestStatus st;
		pc.SetPower(EBTOn, NULL, st);
		r = WaitFor(st, 30000000);
		RDebug::Printf("[btpower] SetPower(on) -> %d", r);
		if (r == KErrTimedOut)
			{
			pc.Cancel();
			User::WaitForRequest(st);
			}
		r = pc.GetPower(state, NULL);
		RDebug::Printf("[btpower] GetPower -> %d, state %d", r, state);
		}
	// the stack's HCI initialisation runs after the H5 link is up (with the firmware download about 2 s more): give it
	// time, then read what it learned
	User::After(8000000);
	RSocketServ ss;
	r = ss.Connect();
	RDebug::Printf("[btpower] RSocketServ::Connect -> %d", r);
	if (r == KErrNone)
		{
		TProtocolDesc pd;
		r = ss.FindProtocol(_L("BTLinkManager"), pd);
		RDebug::Printf("[btpower] FindProtocol(BTLinkManager) -> %d (addr family %u, protocol %u)", r, pd.iAddrFamily, pd.iProtocol);
		RSocket sock;
		r = sock.Open(ss, KBTAddrFamily, KSockBluetoothTypeACL, KBTLinkManager);
		RDebug::Printf("[btpower] ACL link socket open -> %d", r);
		if (r == KErrNone)
			{
			TBTSockAddr local;
			sock.LocalName(local);
			PrintAddr("local BD_ADDR", local.BTAddr());
			sock.Close();
			}
		RHostResolver hr;
		r = hr.Open(ss, KBTAddrFamily, KBTLinkManager);
		RDebug::Printf("[btpower] host resolver open -> %d", r);
		if (r == KErrNone)
			{
			THostName name;
			r = hr.GetHostName(name);
			RDebug::Print(_L("[btpower] local name -> %d '%S'"), r, &name);
			hr.Close();
			}
		ss.Close();
		}
	// keep the power-control session (and with it the stack) alive for a while so a logged HCI exchange can be read
	// (closing it unloads the stack: boot 188 showed teardown crashes in ESock_Bt and btmanserver, an open item)
	User::After(60000000);
	r = pc.GetPower(state, NULL);
	RDebug::Printf("[btpower] DONE GetPower -> %d, state %d", r, state);
	pc.Close();
	delete cleanup;
	return state == EBTOn ? 0 : 1;
	}
