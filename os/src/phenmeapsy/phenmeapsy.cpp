// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phenmeapsy.cpp -- OURS (GNSS rung 2): a serial NMEA positioning module (PSY) for the location server.
//
// Patterned on the 2010 Bluetooth GPS PSY (nlabtgpspsy): the NMEA parser (BTGPSNmeaParser), the fix object
// (BTGPSFix: GGA/GSA/GSV/RMC -> TPositionInfo / TPositionSatelliteInfo) and the NMEA ring buffer are the real source
// files, compiled unmodified from the pinned baseline. What is replaced is the Bluetooth transport and its request
// handler: here the NMEA arrives on the modem's NMEA port (RComm "COMM::9", the Comm.USB unit of USB interface 1) and the
// receiver's GNSS is switched on / off with AT+QGPS=1 / AT+QGPSEND over the modem's second AT port ("COMM::11"; COMM::10
// belongs to the telephony TSY). The modem itself is powered by the TSY: until the ports exist the module reports
// "inactive" and a position request waits.
//
// One CPheNmea per thread (Dll::Tls: an ECOM plug-in DLL has no writable static data) is shared by every positioner.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>
#include <c32comm.h>
#include <ecom/implementationproxy.h>
#include <lbs/epos_cpositioner.h>
#include <lbs/epos_mpositionerstatus.h>
#include <lbspositioninfo.h>
#include <lbssatellite.h>
#include <lbs.h>

#include "BTGPSNmeaParser.h"
#include "BTGPSNmeaBuffer.h"
#include "BTGPSMessageDef.h"
#include "BTGPSFix.h"
#include "BTGPSRequestHandler.h"
#include "BTGPSConstantsManager.h"

static const TUint KPheNmeaPsyImplUid = 0xE7016C20;
static const TInt KNmeaUnit = 9;
static const TInt KAtUnit = 11;
static const TInt KNmeaBufferSize = 4096;
static const TInt KTickUs = 1000000;
_LIT(KCsy, "ECUART");

class CPhePositioner;

// ---- what the real BTGPSFix expects from the Bluetooth request handler: the constants manager (only the user
// equivalent range error is read: BTGPSFix.cpp, FillPositionInfo). The object is plain data here: the real manager
// reads a repository and a resource file and has a private constructor, so memory of its size is used as a record.
struct TPheTls
	{
	CBTGPSConstantsManager* iConstants;
	TAny* iNmea;							// the CPheNmea
	};

CBTGPSConstantsManager& CBTGPSRequestHandler::ConstantsManager()
	{
	return *static_cast<TPheTls*>(Dll::Tls())->iConstants;
	}

// ---------------------------------------------------------------------------------------------------------------
// the NMEA source
// ---------------------------------------------------------------------------------------------------------------
class CPheNmea : public CActive
	{
public:
	static CPheNmea* GetL();
	void Release();
	void RegisterL(CPhePositioner* aPsy);
	void Unregister(CPhePositioner* aPsy);
	void Wake();
	const CBTGPSFix* LastFix() const { return iLast; }
private:
	CPheNmea() : CActive(EPriorityStandard), iRefs(1) {}
	~CPheNmea();
	void ConstructL();
	static TInt TickCb(TAny* aThis);
	void Tick();
	TBool TryOpen();
	TInt Command(const char* aCmd, TInt aTimeoutMs, TDes8* aReply);
	void Read();
	void CloseAll();
	void Process(const TDesC8& aData);
	void Inform();
	void Report(TPositionModuleStatus::TDeviceStatus aDev, TPositionModuleStatus::TDataQualityStatus aQ);
	// from CActive
	void RunL();
	void DoCancel();
private:
	TInt iRefs;
	RCommServ iSrv;
	TBool iSrvOk, iCsyTried, iOpen, iGnss;
	RComm iNmea, iAt;
	CPeriodic* iTick;
	TInt iTicks;
	CBTGPSNmeaBuffer* iBuf;
	CBTGPSFix* iFix;
	CBTGPSFix* iLast;
	TBuf8<256> iRd;
	TBuf8<KMaxNmeaMessageSize> iLine;
	RPointerArray<CPhePositioner> iPsy;
	TPositionModuleStatus::TDeviceStatus iDev;
	TPositionModuleStatus::TDataQualityStatus iQual;
	friend class CPhePositioner;
	};

class CPhePositioner : public CPositioner
	{
public:
	static CPhePositioner* NewL(TAny* aParams);
	~CPhePositioner();
	// from CPositioner
	void NotifyPositionUpdate(TPositionInfoBase& aPosInfo, TRequestStatus& aStatus);
	void CancelNotifyPositionUpdate();
	// from the source
	void Offer(const CBTGPSFix& aFix);
	void ReportStatus(const TPositionModuleStatus& aStatus);
	TBool Pending() const { return iStatus != NULL; }
private:
	CPhePositioner() {}
	void ConstructL(TAny* aParams);
	void ClearPositionInfo(TPositionInfoBase& aInfo);
	void Finish(TInt aErr);
private:
	CPheNmea* iNmea;
	TPositionInfoBase* iPosInfo;
	TRequestStatus* iStatus;
	};

// ---------------------------------------------------------------------------------------------------------------
CPheNmea* CPheNmea::GetL()
	{
	TPheTls* t = static_cast<TPheTls*>(Dll::Tls());
	if (t && t->iNmea)
		{
		CPheNmea* n = static_cast<CPheNmea*>(t->iNmea);
		++n->iRefs;
		return n;
		}
	t = static_cast<TPheTls*>(User::AllocZL(sizeof(TPheTls)));
	CleanupStack::PushL(t);
	// the "constants manager": zeroed memory of its size with the one field BTGPSFix reads (8 m, as the Bluetooth PSY's repository)
	t->iConstants = static_cast<CBTGPSConstantsManager*>(User::AllocZL(sizeof(CBTGPSConstantsManager)));
	t->iConstants->iUserEquivalentRangeError = 8.0;
	t->iConstants->iNmeaBufferSize = KNmeaBufferSize;
	t->iConstants->iSatelliteInfoLifeTime = 60000000;
	User::LeaveIfError(Dll::SetTls(t));
	CleanupStack::Pop(t);
	CPheNmea* n = new (ELeave) CPheNmea;
	CleanupStack::PushL(n);
	n->ConstructL();
	CleanupStack::Pop(n);
	t->iNmea = n;
	return n;
	}

void CPheNmea::ConstructL()
	{
	CActiveScheduler::Add(this);
	iBuf = CBTGPSNmeaBuffer::NewL(KNmeaBufferSize);
	iFix = new (ELeave) CBTGPSFix();
	iFix->SetNmeaBuffer(iBuf);
	iLast = new (ELeave) CBTGPSFix();
	iLast->SetNmeaBuffer(iBuf);
	iTick = CPeriodic::NewL(CActive::EPriorityLow);
	iTick->Start(KTickUs, KTickUs, TCallBack(TickCb, this));
	iDev = TPositionModuleStatus::EDeviceInactive;
	iQual = TPositionModuleStatus::EDataQualityUnknown;
	RDebug::Printf("PH: nmeapsy source created");
	}

void CPheNmea::Release()
	{
	if (--iRefs > 0)
		return;
	TPheTls* t = static_cast<TPheTls*>(Dll::Tls());
	if (t)
		{
		User::Free(t->iConstants);
		t->iNmea = NULL;
		Dll::SetTls(NULL);
		User::Free(t);
		}
	delete this;
	}

CPheNmea::~CPheNmea()
	{
	Cancel();
	delete iTick;
	CloseAll();
	delete iFix;
	delete iLast;
	delete iBuf;
	iPsy.Close();
	}

void CPheNmea::RegisterL(CPhePositioner* aPsy)
	{
	User::LeaveIfError(iPsy.Append(aPsy));
	}

void CPheNmea::Unregister(CPhePositioner* aPsy)
	{
	const TInt i = iPsy.Find(aPsy);
	if (i >= 0)
		iPsy.Remove(i);
	}

void CPheNmea::Wake()
	{
	if (!iOpen)
		TryOpen();
	}

TInt CPheNmea::TickCb(TAny* aThis)
	{
	static_cast<CPheNmea*>(aThis)->Tick();
	return 0;
	}

void CPheNmea::Tick()
	{
	++iTicks;
	if (!iOpen && iTicks % 2 == 0)
		TryOpen();
	}

void CPheNmea::Report(TPositionModuleStatus::TDeviceStatus aDev, TPositionModuleStatus::TDataQualityStatus aQ)
	{
	if (aDev == iDev && aQ == iQual)
		return;
	iDev = aDev;
	iQual = aQ;
	TPositionModuleStatus st;
	st.SetDeviceStatus(aDev);
	st.SetDataQualityStatus(aQ);
	for (TInt i = 0; i < iPsy.Count(); ++i)
		iPsy[i]->ReportStatus(st);
	RDebug::Printf("PH: nmeapsy status device %d quality %d", (TInt)aDev, (TInt)aQ);
	}

void CPheNmea::CloseAll()
	{
	if (iGnss && iAt.SubSessionHandle())
		Command("AT+QGPSEND", 3000, NULL);
	iGnss = EFalse;
	if (iNmea.SubSessionHandle())
		iNmea.Close();
	if (iAt.SubSessionHandle())
		iAt.Close();
	if (iSrvOk)
		{
		iSrv.Close();
		iSrvOk = EFalse;
		}
	iOpen = EFalse;
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

// one AT command on the second AT port, bounded; KErrNone on OK
TInt CPheNmea::Command(const char* aCmd, TInt aTimeoutMs, TDes8* aReply)
	{
	TBuf8<64> out((const TUint8*)aCmd);
	out.Append('\r');
	TRequestStatus ws;
	iAt.Write(ws, out);
	User::WaitForRequest(ws);
	TBuf8<256> acc, buf;
	TTime t0; t0.UniversalTime();
	TInt r = KErrTimedOut;
	for (;;)
		{
		TTime now; now.UniversalTime();
		if ((TInt)(now.MicroSecondsFrom(t0).Int64() / 1000) >= aTimeoutMs)
			break;
		TInt n = ReadFor(iAt, buf, 200000);
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
	if (aReply)
		aReply->Copy(acc.Left(Min(acc.Length(), aReply->MaxLength())));
	return r;
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
	r = aPort.SetConfig(cfg);
	if (r != KErrNone)
		aPort.Close();
	return r;
	}

// open both ports (they exist once the modem has enumerated), switch GNSS on, start reading
TBool CPheNmea::TryOpen()
	{
	if (iOpen)
		return ETrue;
	if (!iSrvOk)
		{
		if (iSrv.Connect() != KErrNone)
			return EFalse;
		iSrvOk = ETrue;
		}
	if (!iCsyTried)
		{
		iCsyTried = ETrue;
		const TInt r = iSrv.LoadCommModule(KCsy);
		RDebug::Printf("PH: nmeapsy LoadCommModule(ECUART) -> %d", r);
		}
	if (!iNmea.SubSessionHandle() && OpenPort(iSrv, iNmea, KNmeaUnit) != KErrNone)
		return EFalse;
	if (!iAt.SubSessionHandle() && OpenPort(iSrv, iAt, KAtUnit) != KErrNone)
		return EFalse;
	RDebug::Printf("PH: nmeapsy ports COMM::%d (NMEA) and COMM::%d (AT) open", KNmeaUnit, KAtUnit);
	TInt r = KErrGeneral;
	for (TInt k = 0; k < 3 && r != KErrNone; ++k)
		r = Command("AT", 2000, NULL);
	TBuf8<200> rep;
	if (r == KErrNone)
		{
		Command("ATE0", 2000, NULL);
		Command("AT+QGPSCFG=\"outport\"", 2000, &rep);
		if (rep.Find(_L8("usbnmea")) < 0)
			Command("AT+QGPSCFG=\"outport\",\"usbnmea\"", 2000, NULL);
		r = Command("AT+QGPS=1", 4000, &rep);
		iGnss = r == KErrNone || rep.Find(_L8("504")) >= 0;		// 504: a GNSS session is already running
		}
	RDebug::Printf("PH: nmeapsy AT port %s, GNSS %s", r == KErrNone ? "answers" : "silent", iGnss ? "on" : "not started");
	if (!iGnss)
		{
		CloseAll();
		Report(TPositionModuleStatus::EDeviceError, TPositionModuleStatus::EDataQualityUnknown);
		return EFalse;
		}
	iOpen = ETrue;
	iLine.Zero();
	Report(TPositionModuleStatus::EDeviceActive, TPositionModuleStatus::EDataQualityLoss);
	Read();
	return ETrue;
	}

void CPheNmea::Read()
	{
	iRd.Zero();
	iNmea.ReadOneOrMore(iStatus, iRd);
	SetActive();
	}

void CPheNmea::DoCancel()
	{
	iNmea.ReadCancel();
	}

void CPheNmea::RunL()
	{
	const TInt err = iStatus.Int();
	if (err != KErrNone && err != KErrTimedOut)
		{
		RDebug::Printf("PH: nmeapsy NMEA read error %d: ports closed, retried", err);
		CloseAll();
		Report(TPositionModuleStatus::EDeviceInactive, TPositionModuleStatus::EDataQualityLoss);
		return;
		}
	Process(iRd);
	Read();
	}

// split the stream into sentences, as BTGPSMessageReceiver does; each valid one goes to the buffer and the fix
void CPheNmea::Process(const TDesC8& aData)
	{
	for (TInt i = 0; i < aData.Length(); ++i)
		{
		const TUint8 c = aData[i];
		if (c == '\r' || c == '\n')
			{
			if (iLine.Length() > 0)
				{
				TBTGPSNmeaParser parser;
				parser.SetNmeaSentence(iLine);
				if (parser.IsValid())
					{
					iBuf->AddSentences(parser.NmeaSentence());
					iBuf->AddSentences(_L8("\r\n"));
					if (iFix->ParseMessage(parser) == CBTGPSFix::EInfoUpdated && iFix->IfFullNmeaPatternReceived())
						Inform();
					}
				iLine.Zero();
				}
			}
		else
			{
			if (iLine.Length() >= KMaxNmeaMessageSize)
				iLine.Zero();
			iLine.Append(c);
			}
		}
	}

// a full GSA + GGA + RMC set: the module status follows the fix, pending requests are answered
void CPheNmea::Inform()
	{
	const CBTGPSFix::TState st = iFix->FixState();
	const TBool valid = st == CBTGPSFix::EFixValid2D || st == CBTGPSFix::EFixValid3D;
	Report(TPositionModuleStatus::EDeviceActive,
		   valid ? TPositionModuleStatus::EDataQualityNormal : TPositionModuleStatus::EDataQualityPartial);
	for (TInt i = iPsy.Count() - 1; i >= 0; --i)
		if (iPsy[i]->Pending())
			iPsy[i]->Offer(*iFix);
	if (iFix->IsFixValid())
		*iLast = *iFix;
	iFix->ResetFix();
	}

// ---------------------------------------------------------------------------------------------------------------
// the positioner (one per RPositioner subsession)
// ---------------------------------------------------------------------------------------------------------------
CPhePositioner* CPhePositioner::NewL(TAny* aParams)
	{
	CPhePositioner* self = new (ELeave) CPhePositioner;
	CleanupStack::PushL(self);
	self->ConstructL(aParams);
	CleanupStack::Pop(self);
	return self;
	}

void CPhePositioner::ConstructL(TAny* aParams)
	{
	BaseConstructL(aParams);
	iNmea = CPheNmea::GetL();
	iNmea->RegisterL(this);
	}

CPhePositioner::~CPhePositioner()
	{
	if (iNmea)
		{
		iNmea->Unregister(this);
		iNmea->Release();
		}
	}

void CPhePositioner::ReportStatus(const TPositionModuleStatus& aStatus)
	{
	if (MPositionerStatus* s = PositionerStatus())
		s->ReportStatus(aStatus);
	}

void CPhePositioner::ClearPositionInfo(TPositionInfoBase& aInfo)
	{
	// as the Bluetooth PSY: reset the caller's object in place, by class
	if (aInfo.PositionClassType() & EPositionSatelliteInfoClass)
		(void) new (&aInfo) (TPositionSatelliteInfo);
	else if (aInfo.PositionClassType() & EPositionCourseInfoClass)
		(void) new (&aInfo) (TPositionCourseInfo);
	else if (aInfo.PositionClassType() & EPositionGenericInfoClass)
		static_cast<HPositionGenericInfo*>(&aInfo)->ClearPositionData();
	else if (aInfo.PositionClassType() & EPositionInfoClass)
		(void) new (&aInfo) (TPositionInfo);
	aInfo.SetModuleId(ImplementationUid());
	}

void CPhePositioner::NotifyPositionUpdate(TPositionInfoBase& aPosInfo, TRequestStatus& aStatus)
	{
	ClearPositionInfo(aPosInfo);
	aStatus = KRequestPending;
	iPosInfo = &aPosInfo;
	iStatus = &aStatus;
	// a valid last fix that is young enough answers at once
	TTime maxAge;
	GetMaxAge(maxAge);
	const CBTGPSFix* last = iNmea->LastFix();
	if (maxAge.Int64() != 0 && last && last->IsFixValid() && last->FixTime() >= maxAge)
		{
		Finish(last->FillPositionInfo(*iPosInfo));
		return;
		}
	iNmea->Wake();
	}

void CPhePositioner::CancelNotifyPositionUpdate()
	{
	if (iStatus)
		Finish(KErrCancel);
	}

void CPhePositioner::Finish(TInt aErr)
	{
	TRequestStatus* s = iStatus;
	iStatus = NULL;
	iPosInfo = NULL;
	if (s)
		User::RequestComplete(s, aErr);
	}

// a fix from the receiver: a valid one completes the request; an invalid one only a client that accepts partial updates
// (satellite information without a position), as BTGPSPositioner::RequestCompleteNotify
void CPhePositioner::Offer(const CBTGPSFix& aFix)
	{
	TInt err = KErrNone;
	switch (aFix.FixState())
		{
	case CBTGPSFix::EFixValid2D:
	case CBTGPSFix::EFixValid3D:
		break;
	default:
		if (!IsPartialUpdateAllowed())
			return;
		err = KPositionPartialUpdate;
		break;
		}
	const TInt fill = aFix.FillPositionInfo(*iPosInfo);
	if (fill != KErrNone)
		err = fill;
	Finish(err);
	}

// ---------------------------------------------------------------------------------------------------------------
static CPositioner* CreatePositioner(TAny* aParams)
	{
	return CPhePositioner::NewL(aParams);
	}

static const TImplementationProxy KFactory[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheNmeaPsyImplUid, CreatePositioner)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KFactory) / sizeof(KFactory[0]);
	return KFactory;
	}
