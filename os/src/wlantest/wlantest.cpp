// wlantest.cpp -- OURS (2026-10-10, PinePhone WiFi W3): does the 2010 WLAN stack see networks through our WHA PDD?
// Through the public management client API (CWlanMgmtClient, the wlanmgmtimpl ECOM plug-in -> RWLMServer ->
// wlmserver -> wlanldd/the UMAC -> wlan.phys -> the rtw88 chip core): a broadcast scan every round, the networks
// found listed as [wlantest] lines (A64_RDEBUG_LOG): BSSID, RX level (RCPI), channel, capability, security, SSID.
// Ten rounds, 20 s apart; the first round also loads the drivers (wlmserver: LoadPhysicalDevice/LoadLogicalDevice +
// RWlanLogicalChannel::Open -> the UMAC's init phase against the PDD).
#include <e32base.h>
#include <e32debug.h>
#include <ecom/ecom.h>
#include <wlanmgmtclient.h>
#include <wlanscaninfo.h>

static const TInt KRounds = 10;
static const TInt KPauseSeconds = 20;
static const TUint8 KIeSsid = 0;
static const TUint8 KIeDsParams = 3;

static void ListL(CWlanScanInfo& aInfo)
	{
	TInt n = 0;
	for (aInfo.First(); !aInfo.IsDone(); aInfo.Next())
		{
		TWlanBssid bssid;
		aInfo.Bssid(bssid);
		TUint8 ssidLen = 0; const TUint8* ssid = NULL;
		TBuf8<33> s;
		if (aInfo.InformationElement(KIeSsid, ssidLen, &ssid) == KErrNone && ssid)
			{
			for (TInt i = 0; i < ssidLen && i < 32; ++i)
				s.Append((ssid[i] >= 32 && ssid[i] < 127) ? ssid[i] : '.');
			}
		TUint8 dsLen = 0; const TUint8* ds = NULL;
		TInt channel = (aInfo.InformationElement(KIeDsParams, dsLen, &ds) == KErrNone && ds && dsLen >= 1) ? ds[0] : 0;
		RDebug::Printf("[wlantest]   %02x:%02x:%02x:%02x:%02x:%02x rx=%u ch=%d cap=0x%04x sec=%d beacon=%u ssid=\"%S\"%s",
					   bssid.Length() > 5 ? bssid[0] : 0, bssid.Length() > 5 ? bssid[1] : 0, bssid.Length() > 5 ? bssid[2] : 0,
					   bssid.Length() > 5 ? bssid[3] : 0, bssid.Length() > 5 ? bssid[4] : 0, bssid.Length() > 5 ? bssid[5] : 0,
					   (TUint)aInfo.RXLevel(), channel, (TUint)aInfo.Capability(), (TInt)aInfo.SecurityMode(),
					   (TUint)aInfo.BeaconInterval(), &s, ssidLen ? "" : " (hidden)");
		++n;
		}
	RDebug::Printf("[wlantest] %d networks (%u bytes of scan data)", n, (TUint)aInfo.Size());
	}

static void RunL()
	{
	CWlanMgmtClient* client = CWlanMgmtClient::NewL();
	CleanupStack::PushL(client);
	CWlanScanInfo* info = CWlanScanInfo::NewL();
	CleanupStack::PushL(info);
	for (TInt round = 1; round <= KRounds; ++round)
		{
		RDebug::Printf("[wlantest] round %d: GetScanResults...", round);
		TTime t0; t0.UniversalTime();
		TInt r = client->GetScanResults(*info);
		TTime t1; t1.UniversalTime();
		RDebug::Printf("[wlantest] round %d: r=%d after %d ms", round, r, (TInt)(t1.MicroSecondsFrom(t0).Int64() / 1000));
		if (r == KErrNone)
			ListL(*info);
		User::After(KPauseSeconds * 1000000);
		}
	CleanupStack::PopAndDestroy(2);
	}

TInt E32Main()
	{
	__UHEAP_MARK;
	CTrapCleanup* cleanup = CTrapCleanup::New();
	CActiveScheduler* sched = new CActiveScheduler;
	CActiveScheduler::Install(sched);
	RDebug::Printf("[wlantest] start");
	TRAPD(err, RunL());
	RDebug::Printf("[wlantest] done err=%d", err);
	REComSession::FinalClose();
	delete sched;
	delete cleanup;
	__UHEAP_MARKEND;
	return err;
	}
