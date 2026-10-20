// btengon.cpp -- OURS (2026-10-10, Bluetooth rung R4): the S60 way of switching Bluetooth on, as BtUi / the toggle do:
// CBTEngSettings::SetPowerState(EBTPowerOn) -> btengsrv.exe (started on demand) -> RBTPowerControl -> the stack
// (bt_v2.prt -> corehci -> hctl_h5 + initialisor_rtl). Then the visibility and the local name through the same API,
// and the power state read back. SSM_TEST=btengon stages it as ssmtest.exe (the SSM's non-critical list).
#include <e32base.h>
#include <e32debug.h>
#include <btengsettings.h>
#include <btengconstants.h>
#include <btserversdkcrkeys.h>

static void RunL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;		// CBTEngSettings keeps active objects
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	CBTEngSettings* settings = CBTEngSettings::NewLC();
	TBTPowerStateValue power = EBTPowerOff;
	TInt r = settings->GetPowerState(power);
	RDebug::Printf("[btengon] GetPowerState -> %d, state %d (1 = on)", r, power);
	// ALWAYS ask for power on (boot 218, the user's test): GetPowerState reads the CenRep key the previous boot left
	// at "on", but nothing restores the stack from it at boot (on a phone SysAp does) -- the engine compares the
	// request with the HARDWARE state (RBTPowerControl::GetPower) and ignores a true no-op, so this is idempotent
	r = settings->SetPowerState(EBTPowerOn);
	RDebug::Printf("[btengon] SetPowerState(on) -> %d", r);
	// the engine turns the controller on asynchronously (firmware download + the stack's init: ~3 s)
	User::After(10000000);
	r = settings->GetPowerState(power);
	RDebug::Printf("[btengon] GetPowerState -> %d, state %d", r, power);
	TBTVisibilityMode vis = EBTVisibilityModeNoScans;
	r = settings->GetVisibilityMode(vis);
	RDebug::Printf("[btengon] GetVisibilityMode -> %d, mode %d (3 = general discoverable)", r, vis);
	if (r == KErrNone && vis != EBTVisibilityModeGeneral)
		{
		r = settings->SetVisibilityMode(EBTVisibilityModeGeneral);
		RDebug::Printf("[btengon] SetVisibilityMode(general) -> %d", r);
		}
	TBuf<64> name;
	r = settings->GetLocalName(name);
	TBuf8<64> n8;
	n8.Copy(name);
	RDebug::Printf("[btengon] GetLocalName -> %d '%S'", r, &n8);
	if (r == KErrNone && name.Length() == 0)
		{
		name.Copy(_L("Phorepoc PinePhone"));
		r = settings->SetLocalName(name);
		RDebug::Printf("[btengon] SetLocalName -> %d", r);
		}
	// hold the engine session open: when its last client goes the engine exits after 3 s (KBTEngSrvIdleTimeout) and
	// takes the stack down with it (RBTPowerControl::Close stops the protocol) -- the phone's SysAp is that client
	// for the whole uptime; here we are, for two hours, logging the state once a minute
	for (TInt minute = 1; minute <= 120; ++minute)
		{
		User::After(60000000);
		r = settings->GetPowerState(power);
		RDebug::Printf("[btengon] minute %d: GetPowerState -> %d, state %d", minute, r, power);
		}
	r = settings->GetPowerState(power);
	RDebug::Printf("[btengon] DONE GetPowerState -> %d, state %d", r, power);
	CleanupStack::PopAndDestroy(2);		// settings, sched
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[btengon] start");
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	TRAPD(err, RunL());
	RDebug::Printf("[btengon] exit %d", err);
	delete cleanup;
	return err;
	}
