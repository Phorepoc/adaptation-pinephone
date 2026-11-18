// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phesimtest.cpp -- OURS (pheltsy SIM rung): what the Phone application sees of the SIM and the network, through ETel.
//   SIM        : the security event (a card with no PIN raises none, an empty slot raises ENoICCFound), the ICC lock info
//                (answered by the TSY, not by the card), the subscriber id (IMSI: only a usable SIM has one) and the ICC
//                identity (the 2010 CTSY does not implement it: an error is the expected answer)
//   network    : registration status (waited for up to 90 s), the current network's names / MCC / MNC, signal strength
// Every wait is bounded. IMSI and IMEI are NEVER logged whole: the last four digits and the length only.
#include <e32base.h>
#include <e32debug.h>
#include <etel.h>
#include <etelmm.h>

static const TInt KStepTimeout = 15 * 1000000;

static TInt Await(TRequestStatus& aStatus, const char* aWhat, TInt aTimeout = KStepTimeout)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, aTimeout);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("PH: simtest %s: TIMED OUT", aWhat);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	RDebug::Printf("PH: simtest %s: %d", aWhat, aStatus.Int());
	return aStatus.Int();
	}

static TInt RunTest()
	{
	RTelServer srv;
	TInt r = srv.Connect();
	RDebug::Printf("PH: simtest RTelServer::Connect -> %d", r);
	if (r != KErrNone)
		return r;
	TInt phones = 0;
	for (TInt i = 0; i < 60 && phones < 1; ++i)
		{
		srv.EnumeratePhones(phones);
		if (phones < 1)
			User::After(500000);
		}
	RTelServer::TPhoneInfo info;
	r = srv.GetPhoneInfo(0, info);
	RDebug::Printf("PH: simtest %d phone(s), GetPhoneInfo -> %d", phones, r);
	RMobilePhone phone;
	if (r == KErrNone)
		r = phone.Open(srv, info.iName);
	RDebug::Printf("PH: simtest RMobilePhone::Open -> %d", r);
	if (r != KErrNone)
		return r;
	TRequestStatus st;

	// ---- the SIM: a security event within 10 s (a SIM without a PIN raises none; an empty slot: ENoICCFound)
	RMobilePhone::TMobilePhoneSecurityEvent ev = RMobilePhone::ENoICCFound;
	phone.NotifySecurityEvent(st, ev);
	r = Await(st, "NotifySecurityEvent", 10 * 1000000);
	TBool noSim = EFalse;
	if (r == KErrTimedOut)
		{
		phone.CancelAsyncRequest(EMobilePhoneNotifySecurityEvent);
		User::WaitForRequest(st);
		RDebug::Printf("PH: simtest no security event in 10 s (a SIM without a PIN raises none)");
		}
	else
		{
		RDebug::Printf("PH: simtest security event %d", (TInt)ev);
		noSim = r == KErrNone && ev == RMobilePhone::ENoICCFound;
		}

	RMobilePhone::TMobilePhoneLockInfoV1 lock;
	RMobilePhone::TMobilePhoneLockInfoV1Pckg lockP(lock);
	phone.GetLockInfo(st, RMobilePhone::ELockICC, lockP);
	r = Await(st, "GetLockInfo(ICC)");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetLockInfo);
	else
		RDebug::Printf("PH: simtest ICC lock: status %d setting %d (answered by the TSY)", (TInt)lock.iStatus, (TInt)lock.iSetting);

	RMobilePhone::TMobilePhoneSubscriberId imsi;
	phone.GetSubscriberId(st, imsi);
	r = Await(st, "GetSubscriberId");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetSubscriberId);
	TBuf<8> tail(imsi.Right(4));
	RDebug::Print(_L("PH: simtest IMSI ...%S (%d digits)"), &tail, imsi.Length());
	const TBool imsiOk = r == KErrNone && imsi.Length() >= 14 && imsi.Length() <= 15;

	RMobilePhone::TMobilePhoneIdentityV1 id;
	phone.GetPhoneId(st, id);
	r = Await(st, "GetPhoneId");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetPhoneId);
	TBuf<8> itail(id.iSerialNumber.Right(4));
	RDebug::Print(_L("PH: simtest IMEI ...%S (%d digits)"), &itail, id.iSerialNumber.Length());

	RMobilePhone::TIccIdentity icc;
	phone.GetIccIdentity(st, icc);
	r = Await(st, "GetIccIdentity (the 2010 CTSY has no handler)");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetIccIdentity);
	RDebug::Printf("PH: simtest ICC identity: %d bytes (never logged)", icc.Length());

	// ---- the network: registered within 90 s (3 s polls)
	RMobilePhone::TMobilePhoneRegistrationStatus reg = RMobilePhone::ERegistrationUnknown;
	TInt last = -1;
	TBool registered = EFalse;
	TTime t0; t0.UniversalTime();
	for (TInt k = 0; k < 30 && !registered; ++k)
		{
		phone.GetNetworkRegistrationStatus(st, reg);
		r = Await(st, "GetNetworkRegistrationStatus", 5 * 1000000);
		if (r == KErrTimedOut)
			phone.CancelAsyncRequest(EMobilePhoneGetNetworkRegistrationStatus);
		if ((TInt)reg != last)
			{
			TTime now; now.UniversalTime();
			RDebug::Printf("PH: simtest registration status %d after %d s (1 not registered, 2 searching, 3 busy, 4 denied, 5 home, 6 unknown, 7 roaming)",
						   (TInt)reg, (TInt)(now.MicroSecondsFrom(t0).Int64() / 1000000));
			last = (TInt)reg;
			}
		registered = reg == RMobilePhone::ERegisteredOnHomeNetwork || reg == RMobilePhone::ERegisteredRoaming;
		if (!registered)
			User::After(3000000);
		}
	RMobilePhone::TMobilePhoneNetworkInfoV1 net;
	RMobilePhone::TMobilePhoneNetworkInfoV1Pckg netP(net);
	RMobilePhone::TMobilePhoneLocationAreaV1 area;
	phone.GetCurrentNetwork(st, netP, area);
	r = Await(st, "GetCurrentNetwork");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetCurrentNetwork);
	RDebug::Print(_L("PH: simtest network: long '%S' short '%S' mcc %S mnc %S mode %d status %d"),
				  &net.iLongName, &net.iShortName, &net.iCountryCode, &net.iNetworkId, (TInt)net.iMode, (TInt)net.iStatus);
	const TBool nameOk = r == KErrNone && net.iLongName.Length() > 0;

	TInt32 rssi = 0;
	TInt8 bars = 0;
	phone.GetSignalStrength(st, rssi, bars);
	r = Await(st, "GetSignalStrength");
	if (r == KErrTimedOut)
		phone.CancelAsyncRequest(EMobilePhoneGetSignalStrength);
	RDebug::Printf("PH: simtest signal: %d dBm, %d of 7 bars", (TInt)rssi, (TInt)bars);
	const TBool signalOk = r == KErrNone && bars > 0;

	phone.Close();
	srv.Close();
	RDebug::Printf("PH: simtest summary: SIM %s, IMSI %s, registered %s, operator name %s, signal %s",
				   noSim ? "ABSENT (ENoICCFound)" : "present", imsiOk ? "ok" : "MISSING",
				   registered ? "yes" : "NO", nameOk ? "ok" : "MISSING", signalOk ? "ok" : "NONE");
	const TBool pass = !noSim && imsiOk && registered && nameOk && signalOk;
	RDebug::Printf("PH: simtest done (%s)", pass ? "PASS" : "FAIL");
	return pass ? KErrNone : KErrGeneral;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	CActiveScheduler* scheduler = new CActiveScheduler;
	if (scheduler)
		CActiveScheduler::Install(scheduler);
	RDebug::Printf("PH: simtest start");
	const TInt r = RunTest();
	delete scheduler;
	delete cleanup;
	return r;
	}
