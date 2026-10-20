// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// senstest.cpp -- OURS (2026-10-10, PinePhone sensors rung S2): does the 2010 sensor server see our SSY (ssya64) and
// its LDD (Sensors.A64)? Through the public client API (sensrvclient): find the accelerometer XYZ channels, open the
// first, listen (1 item at a time) and log 20 samples in the device frame; then find the orientation channel (the RCL_3
// orientationssy on top of ours) and log every orientation it reports for 30 s. [senstest] lines (A64_RDEBUG_LOG).
// S2's pass: samples with |g| ~ 16384 and z ~ +16384 face-up; the orientation channel reporting EOrientationDisplayUp
// with the phone held upright (the decision's "measure the upright sign first" step).
#include <e32base.h>
#include <e32debug.h>
#include <sensrvchannelfinder.h>
#include <sensrvchannel.h>
#include <sensrvdatalistener.h>
#include <sensrvaccelerometersensor.h>
#include <sensrvorientationsensor.h>
#include <sensrvgeneralproperties.h>
#include <centralrepository.h>
#include <e32property.h>
#include <CFClient.h>
#include <CFListener.h>
#include <CFContextQuery.h>
#include <CFContextObject.h>

class CListener : public CBase, public MSensrvDataListener
	{
public:
	CListener(TInt aWanted) : iWanted(aWanted), iGot(0) {}
	void DataReceived(CSensrvChannel& aChannel, TInt aCount, TInt aDataLost)
		{
		const TSensrvChannelInfo& info = aChannel.GetChannelInfo();
		if (info.iChannelType == KSensrvChannelTypeIdAccelerometerXYZAxisData)
			{
			TPckgBuf<TSensrvAccelerometerAxisData> d;
			if (aChannel.GetData(d) == KErrNone)
				RDebug::Printf("[senstest] accel #%d x=%d y=%d z=%d (count %d lost %d)", iGot, d().iAxisX, d().iAxisY, d().iAxisZ, aCount, aDataLost);
			}
		else if (info.iChannelType == KSensrvChannelTypeIdOrientationData)
			{
			TPckgBuf<TSensrvOrientationData> d;
			if (aChannel.GetData(d) == KErrNone)
				RDebug::Printf("[senstest] orientation #%d = %d (1 up, 2 down, 3 left-up, 4 right-up, 5 face-up, 6 face-down)", iGot, (TInt)d().iDeviceOrientation);
			}
		else
			RDebug::Printf("[senstest] data on channel type 0x%08x", info.iChannelType);
		if (++iGot >= iWanted)
			CActiveScheduler::Stop();
		}
	void DataError(CSensrvChannel&, TSensrvErrorSeverity aError)
		{
		RDebug::Printf("[senstest] DataError severity %d", (TInt)aError);
		CActiveScheduler::Stop();
		}
	void GetDataListenerInterfaceL(TUid, TAny*& aInterface) { aInterface = NULL; }
	TInt iWanted, iGot;
	};

class CStopper : public CTimer
	{
public:
	CStopper() : CTimer(EPriorityStandard) { CActiveScheduler::Add(this); }
	void StartL(TInt aSeconds) { ConstructL(); After(aSeconds * 1000000); }
	void RunL() { RDebug::Printf("[senstest] time is up"); CActiveScheduler::Stop(); }
	};

static void RunChannelL(TSensrvChannelTypeId aType, const char* aName, TInt aWanted, TInt aSeconds)
	{
	CSensrvChannelFinder* finder = CSensrvChannelFinder::NewLC();
	RSensrvChannelInfoList list;
	CleanupClosePushL(list);
	TSensrvChannelInfo query;
	query.iChannelType = aType;
	finder->FindChannelsL(list, query);
	RDebug::Printf("[senstest] %s: %d channel(s)", aName, list.Count());
	if (list.Count() == 0)
		{
		CleanupStack::PopAndDestroy(2);
		return;
		}
	const TSensrvChannelInfo& ci = list[0];
	TBuf8<32> loc(ci.iLocation), ven(ci.iVendorId);
	RDebug::Printf("[senstest]   id %d location '%S' vendor '%S' item size %d", ci.iChannelId, &loc, &ven, ci.iDataItemSize);
	CSensrvChannel* ch = CSensrvChannel::NewLC(ci);
	ch->OpenChannelL();
	RDebug::Printf("[senstest]   opened");
	TSensrvProperty rate;
	TRAPD(pe, ch->GetPropertyL(KSensrvPropIdDataRate, KSensrvItemIndexNone, rate));
	if (pe == KErrNone)
		{
		TInt hz = 0, mx = 0;
		rate.GetValue(hz);
		rate.GetMaxValue(mx);
		RDebug::Printf("[senstest]   data rate %d Hz (max %d)", hz, mx);
		}
	else
		RDebug::Printf("[senstest]   GetPropertyL(DataRate) left %d", pe);
	CListener* l = new (ELeave) CListener(aWanted);
	CleanupStack::PushL(l);
	CStopper* stop = new (ELeave) CStopper;
	CleanupStack::PushL(stop);
	stop->StartL(aSeconds);
	ch->StartDataListeningL(l, 1, 1, 0);
	CActiveScheduler::Start();
	stop->Cancel();
	ch->StopDataListening();
	ch->CloseChannel();
	RDebug::Printf("[senstest]   closed after %d item(s)", l->iGot);
	CleanupStack::PopAndDestroy(4);		// stop, l, ch, list
	CleanupStack::PopAndDestroy(finder);
	}

static void PrintSensorServerSettingsL()
	{
	CRepository* rep = CRepository::NewLC(TUid::Uid(0x1020507E));
	TInt stack = -1, heap = -1, timeout = -1;
	rep->Get(2, stack);
	rep->Get(3, heap);
	rep->Get(1, timeout);
	RDebug::Printf("[senstest] CenRep 1020507E: timeout %d, SSY stack %d, SSY heap max %d", timeout, stack, heap);
	CleanupStack::PopAndDestroy(rep);
	}

// the rotation chain's inputs (S2b): every context the autorotateui rules test, as the Context Framework holds them,
// and Avkon's hardware layout state (KPSUidUikon/KUikLayoutState, written by akncapserver on the hardware-state key)
class CCfProbe : public CBase, public MCFListener
	{
public:
	void ContextIndicationL(const CCFContextIndication&) {}
	void ActionIndicationL(const CCFActionIndication&) {}
	void HandleContextFrameworkError(TCFError aError, const TDesC& aSource, const TDesC& aType)
		{
		TBuf8<64> s, t; s.Copy(aSource.Left(60)); t.Copy(aType.Left(60));
		RDebug::Printf("[senstest] CF error %d on %S/%S", (TInt)aError, &s, &t);
		}
	TAny* Extension(const TUid&) const { return NULL; }
	};

static void QueryContextL(CCFClient& aCf, const TDesC& aSource, const TDesC& aType)
	{
	CCFContextQuery* q = CCFContextQuery::NewLC(aSource, aType);
	RContextObjectArray results;
	CleanupClosePushL(results);
	const TInt r = aCf.RequestContext(*q, results);
	TBuf8<64> s, t; s.Copy(aSource); t.Copy(aType);
	if (r != KErrNone || results.Count() == 0)
		RDebug::Printf("[senstest] CF %S/%S: r=%d, %d result(s)", &s, &t, r, results.Count());
	for (TInt i = 0; i < results.Count(); ++i)
		{
		TBuf8<64> v; v.Copy(results[i]->Value().Left(60));
		RDebug::Printf("[senstest] CF %S/%S = '%S'", &s, &t, &v);
		}
	results.ResetAndDestroy();
	CleanupStack::PopAndDestroy(2);
	}

static void DumpRotationChainL()
	{
	TInt layoutState = -1;
	const TInt pr = RProperty::Get(TUid::Uid(0x101F8773), 0x00000008, layoutState);		// KPSUidUikon / KUikLayoutState
	RDebug::Printf("[senstest] KUikLayoutState = %d (r=%d; akncapserver's hardware state)", layoutState, pr);
	CCfProbe* probe = new (ELeave) CCfProbe;
	CleanupStack::PushL(probe);
	CCFClient* cf = CCFClient::NewLC(*probe);
	QueryContextL(*cf, _L("Sensor"), _L("Setting.Active"));
	QueryContextL(*cf, _L("Sensor"), _L("Setting.TurningInteraction.AutoRotation"));
	QueryContextL(*cf, _L("Device"), _L("Display.Status.Light"));
	QueryContextL(*cf, _L("Device"), _L("DeviceLock.Status"));
	QueryContextL(*cf, _L("System"), _L("Keyguard.Status"));
	QueryContextL(*cf, _L("Device"), _L("Orientation.Active"));
	QueryContextL(*cf, _L("Sensor"), _L("Event.Orientation"));
	QueryContextL(*cf, _L("Application"), _L("State.Foreground"));
	CleanupStack::PopAndDestroy(2);
	}

static void MainL()
	{
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	RDebug::Printf("[senstest] === sensor server client test (rung S2) ===");
	// the sensor server's settings as the CenRep server serves them (key 2 = the SSY thread stack; the board override
	// says 16384 -- a 4096 here means the server is serving a stale converted copy from C:)
	TRAPD(ce, PrintSensorServerSettingsL());
	if (ce != KErrNone)
		RDebug::Printf("[senstest] CenRep 1020507E open left %d", ce);
	User::After(3000000);					// let the sensor server's ECOM scan settle
	RunChannelL(KSensrvChannelTypeIdAccelerometerXYZAxisData, "accelerometer XYZ", 20, 20);
	RunChannelL(KSensrvChannelTypeIdOrientationData, "orientation", 100, 30);
	TRAPD(cfe, DumpRotationChainL());
	if (cfe != KErrNone)
		RDebug::Printf("[senstest] DumpRotationChainL left %d", cfe);
	// the rotation bench (2026-10-15): the phone's C: keeps a persisted copy of the sensor settings (10282DF0) that
	// overrides a bench ROM's turning control, so arm auto-rotate HERE for 40 s and put the old value back after
	// (a normal image must never be left auto-rotating into the black rotated frame)
	CRepository* sens = NULL;
	TRAPD(re, sens = CRepository::NewL(TUid::Uid(0x10282DF0)));
	TInt turning = -1;
	if (sens)
		{
		sens->Get(3, turning);
		const TInt sr = sens->Set(3, 4);
		RDebug::Printf("[senstest] rotation bench: turning control %d -> 4 (r=%d); 40 s window", turning, sr);
		User::After(10000000);
		TRAP(cfe, DumpRotationChainL());
		RDebug::Printf("[senstest] rotation bench: window open");
		User::After(30000000);
		const TInt rr = sens->Set(3, turning >= 0 ? turning : 0);
		RDebug::Printf("[senstest] rotation bench: turning control back to %d (r=%d)", turning, rr);
		delete sens;
		}
	else
		RDebug::Printf("[senstest] rotation bench: CenRep 10282DF0 open left %d", re);
	RDebug::Printf("[senstest] === end ===");
	CleanupStack::PopAndDestroy(s);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	if (err != KErrNone)
		RDebug::Printf("[senstest] MainL left %d", err);
	delete cleanup;
	return err;
	}
