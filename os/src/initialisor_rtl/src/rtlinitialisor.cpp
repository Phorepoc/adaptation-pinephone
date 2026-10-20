// Portions Copyright (c) Nokia Corporation and/or its subsidiary(-ies)
// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// rtlinitialisor.cpp -- OURS (Bluetooth rung R3, 2026-10-10): see rtlinitialisor.h. Skeleton from the TI initialisor
// (Nokia, EPL v1.0); the Realtek protocol from Linux btrtl.c / hci_h5.c as documented in
// docs/phorepoc/research/bluetooth-linux.md (1.2 vendor commands, 1.3 patch + config download, 3 the file facts).
/**
 @file
 @internalComponent
*/

#include "rtlinitialisor.h"
#include "hctlconfiginterface.h"

#include <e32debug.h>
#include <f32file.h>
#include <bluetooth/hci/vendordebugcommand.h>
#include <bluetooth/hci/controllerinitialisationobserver.h>
#include <bluetooth/hcicommandqueue.h>
#include <bluetooth/hci/readlocalversioninfocommand.h>
#include <bluetooth/hci/commandcompleteevent.h>
#include <bluetooth/hci/readlocalversioninfocompleteevent.h>
#include <bluetooth/hci/vendordebugcompleteevent.h>
#include <bluetooth/hci/hciopcodes.h>
#include <bluetooth/hci/hcierrors.h>
#include <bluetooth/hci/corehci.h>
#include <bluetooth/hci/hciutil.h>

_LIT(KFwPath, "z:\\private\\101f7989\\bluetooth\\");
_LIT(KFwFile, "rtl8723cs_xx_fw.bin");
_LIT(KConfigFile, "rtl8723cs_xx_config.bin");
_LIT(KIniName, "initialisor_rtl");

const TUint16 KRtlReadRegister   = 0xfc61;		// 5 param bytes; return {status, u16 value}
const TUint16 KRtlReadRomVersion = 0xfc6d;		// return {status, version}
const TUint16 KRtlSetBaudRate    = 0xfc17;		// u32 LE device rate code
const TUint16 KRtlDownload       = 0xfc20;		// {index, data[<=252]}; return {status, index}
const TUint16 KRtl8723csLmpSubversion = 0x8703;
const TUint8  KRtl8723csChipType = 5;			// RTL_CHIP_8723CS_XX
const TInt    KRtlFragment = 252;
const TUint32 KRtlEpatchSignatureLo = 0x6c616552;	// "Real"
const TUint32 KRtlEpatchSignatureHi = 0x68636574;	// "tech"
_LIT8(KRtlChipTypeParams, "\x00\x94\xa0\x00\xb0");
_LIT8(KRtlConfigMagic, "\x55\xab\x23\x87");

static TUint32 RateOfCode(TUint32 aCode)
	{
	// Linux btrtl.c baudrate_to_device (the values seen in Realtek configs)
	switch (aCode)
		{
		case 0x0252c014: return 115200;
		case 0x0252c00a: return 230400;
		case 0x05f75004: return 921600;
		case 0x00005004: return 1000000;
		case 0x04928002:
		case 0x01128002: return 1500000;
		case 0x00005002: return 2000000;
		case 0x0000b001: return 2500000;
		case 0x04928001: return 3000000;
		case 0x052a6001: return 3500000;
		case 0x00005001: return 4000000;
		default: return 0;
		}
	}

/*static*/ CRtlInitialisor* CRtlInitialisor::NewL()
	{
	CRtlInitialisor* self = new (ELeave) CRtlInitialisor();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CRtlInitialisor::ConstructL()
	{
	TCallBack preResetCallBack(AsyncCallBackForPreReset, this);
	iAsyncCallBackForPreReset = new (ELeave)CAsyncCallBack(preResetCallBack, CActive::EPriorityStandard);
	TCallBack postResetCallBack(AsyncCallBackForPostReset, this);
	iAsyncCallBackForPostReset = new (ELeave)CAsyncCallBack(postResetCallBack, CActive::EPriorityStandard);
	iDelay = CRtlDelay::NewL(*this);
	// initialisor_rtl.ini [rtl] baud= : 0 keeps 115200 (no 0xfc17), otherwise the config's rate is used
	iIniBaud = 1;
	TRAPD(err,
		{
		CHciUtil* hciUtil = CHciUtil::NewL(KIniName);
		CleanupStack::PushL(hciUtil);
		hciUtil->OpenIniFileL();
		iIniBaud = hciUtil->GetValueFromFileL(_L("rtl"), _L("baud"));
		CleanupStack::PopAndDestroy(hciUtil);
		});
	if (err != KErrNone)
		iIniBaud = 1;
	RDebug::Printf("[rtlinit] loaded (baud switch %s)", iIniBaud ? "on" : "off");
	}

CRtlInitialisor::CRtlInitialisor()
  : iInitState(EIdle)
	{
	}

CRtlInitialisor::~CRtlInitialisor()
	{
	delete iAsyncCallBackForPreReset;
	delete iAsyncCallBackForPostReset;
	delete iDelay;
	iImage.Close();
	}

/*virtual*/ TAny* CRtlInitialisor::Interface(TUid aUid)
	{
	TAny* ret = NULL;
	switch(aUid.iUid)
		{
		case KControllerInitialisationInterfaceUid:
			ret = reinterpret_cast<TAny*>(static_cast<MControllerInitialisationInterface*>(this));
			break;
		default:
			break;
		};
	return ret;
	}

/*static*/ TInt CRtlInitialisor::AsyncCallBackForPreReset(TAny* aInitPlugin)
	{
	static_cast<CRtlInitialisor*>(aInitPlugin)->DoPreReset();
	return EFalse;
	}

/*static*/ TInt CRtlInitialisor::AsyncCallBackForPostReset(TAny* aInitPlugin)
	{
	static_cast<CRtlInitialisor*>(aInitPlugin)->DoPostReset();
	return EFalse;
	}

void CRtlInitialisor::DoPreReset()
	{
	RDebug::Printf("[rtlinit] pre-reset: Read_Local_Version");
	iSent = 0;
	iImage.Close();
	iInitState = EWaitLocalVersion;
	TRAPD(err, SendReadLocalVersionL());
	if (err != KErrNone)
		Finish(err);
	}

void CRtlInitialisor::DoPostReset()
	{
	// nothing after HCI_Reset: the firmware is in place, the stack's own init follows
	iControllerInitialisationObserver->McioPostResetCommandComplete(KErrNone);
	}

/*virtual*/ void CRtlInitialisor::MciiPreResetCommand()
	{
	__ASSERT_DEBUG(iControllerInitialisationObserver, User::Panic(KRtlInitialisorPanic, ERtlNullInitialisationObserver));
	iAsyncCallBackForPreReset->CallBack();
	}

/*virtual*/ void CRtlInitialisor::MciiPostResetCommand()
	{
	__ASSERT_DEBUG(iControllerInitialisationObserver, User::Panic(KRtlInitialisorPanic, ERtlNullInitialisationObserver));
	iAsyncCallBackForPostReset->CallBack();
	}

/*virtual*/ void CRtlInitialisor::MciiSetHCICommandQueue(MHCICommandQueue& aHCICommandQueue)
	{
	__ASSERT_DEBUG(iHCICommandQueue == 0, User::Panic(KRtlInitialisorPanic, ERtlHciCommandQueueOverwritten));
	iHCICommandQueue = &aHCICommandQueue;
	}

/*virtual*/ void CRtlInitialisor::MciiSetControllerInitialisationObserver(MControllerInitialisationObserver& aObserver)
	{
	__ASSERT_DEBUG(iControllerInitialisationObserver == 0, User::Panic(KRtlInitialisorPanic, ERtlInitialisationObserverOverwritten));
	iControllerInitialisationObserver = &aObserver;
	}

/*virtual*/ void CRtlInitialisor::MciiSetCoreHci(MCoreHci& aCoreHci)
	{
	__ASSERT_DEBUG(iCoreHci == 0, User::Panic(KRtlInitialisorPanic, ERtlCoreHciOverwritten));
	iCoreHci = &aCoreHci;
	}

void CRtlInitialisor::SendReadLocalVersionL()
	{
	CReadLocalVersionInfoCommand* cmd = CReadLocalVersionInfoCommand::NewL();
	CleanupStack::PushL(cmd);
	iHCICommandQueue->MhcqAddInitCommandL(cmd, *this);
	CleanupStack::Pop(cmd);
	}

void CRtlInitialisor::SendVendorL(TUint16 aOpcode, const TDesC8& aParams)
	{
	CVendorDebugCommand* cmd = CVendorDebugCommand::NewL(aOpcode);
	CleanupStack::PushL(cmd);
	cmd->Command().Copy(aParams);
	iHCICommandQueue->MhcqAddInitCommandL(cmd, *this);
	CleanupStack::Pop(cmd);
	}

void CRtlInitialisor::Finish(TInt aError)
	{
	RDebug::Printf("[rtlinit] pre-reset done: %d", aError);
	iInitState = EIdle;
	iDelay->Cancel();
	iImage.Close();
	iControllerInitialisationObserver->McioPreResetCommandComplete(aError);
	}

void CRtlInitialisor::MhcqcCommandEventReceived(const THCIEventBase& aEvent, const CHCICommandBase* /*aRelatedCommand*/)
	{
	if (aEvent.EventCode() != ECommandCompleteEvent)
		return;
	THCICommandCompleteEvent& event = THCICommandCompleteEvent::Cast(aEvent);
	if (event.CommandOpcode() == KReadLocalVersionInfoOpcode)
		HandleLocalVersion(TReadLocalVersionInfoCompleteEvent::Cast(event));
	else if ((event.CommandOpcode() & KVendorDebugOGF) == KVendorDebugOGF)
		HandleVendorComplete(TVendorDebugCompleteEvent::Cast(event));
	}

void CRtlInitialisor::MhcqcCommandErrored(TInt aErrorCode, const CHCICommandBase* /*aCommand*/)
	{
	RDebug::Printf("[rtlinit] command errored %d in state %d", aErrorCode, iInitState);
	if (iInitState != EIdle)
		Finish(aErrorCode);
	}

void CRtlInitialisor::HandleLocalVersion(const TReadLocalVersionInfoCompleteEvent& aEvent)
	{
	if (aEvent.ErrorCode() != EOK)
		{
		Finish(KHCIErrorBase - aEvent.ErrorCode());
		return;
		}
	RDebug::Printf("[rtlinit] local version: hci %d rev %04x lmp %d subver %04x manufacturer %d",
	               aEvent.Version(), aEvent.Revision(), aEvent.LMPVersion(), aEvent.LMPSubversion(), aEvent.ManufacturerName());
	if (iInitState == EWaitPatchedVersion)
		{
		Finish(KErrNone);
		return;
		}
	if (iInitState != EWaitLocalVersion)
		return;
	iLmpSubversion = aEvent.LMPSubversion();
	if (iLmpSubversion != KRtl8723csLmpSubversion)
		{
		// not the ROM firmware of an 8723CS: already patched (no power cycle since), or another chip -- leave it
		RDebug::Printf("[rtlinit] not an unpatched RTL8723CS: no download");
		Finish(KErrNone);
		return;
		}
	iInitState = EWaitChipType;
	TRAPD(err, SendVendorL(KRtlReadRegister, KRtlChipTypeParams));
	if (err != KErrNone)
		Finish(err);
	}

void CRtlInitialisor::HandleVendorComplete(const TVendorDebugCompleteEvent& aEvent)
	{
	const TPtrC8 data(aEvent.VendorDebugData());		// {status, ...}
	const TUint8 status = data.Length() ? data[0] : 0xff;
	TInt err = KErrNone;
	switch (iInitState)
		{
		case EWaitChipType:
			iChipType = static_cast<TUint8>(data.Length() > 1 ? data[1] & 0x0f : 0);
			RDebug::Printf("[rtlinit] chip type: status %d type %d%s", status, iChipType, iChipType == KRtl8723csChipType ? " (8723CS)" : " (UNEXPECTED, going on)");
			iInitState = EWaitRomVersion;
			TRAP(err, SendVendorL(KRtlReadRomVersion, KNullDesC8));
			break;
		case EWaitRomVersion:
			iRomVersion = static_cast<TUint8>(data.Length() > 1 ? data[1] : 0);
			RDebug::Printf("[rtlinit] rom version: status %d version %d", status, iRomVersion);
			if (status != 0)
				{
				err = KErrCorrupt;
				break;
				}
			TRAP(err, LoadFirmwareL(iRomVersion));
			if (err != KErrNone)
				break;
			if (iIniBaud && iBaudCode && iBaudRate && iBaudRate != 115200)
				{
				TBuf8<4> code;
				code.SetLength(4);
				code[0] = static_cast<TUint8>(iBaudCode);
				code[1] = static_cast<TUint8>(iBaudCode >> 8);
				code[2] = static_cast<TUint8>(iBaudCode >> 16);
				code[3] = static_cast<TUint8>(iBaudCode >> 24);
				RDebug::Printf("[rtlinit] baud: code %08x -> %u bps, flow control %d", iBaudCode, iBaudRate, iFlowControl);
				iInitState = EWaitBaudChange;
				TRAP(err, SendVendorL(KRtlSetBaudRate, code));
				}
			else
				{
				TRAP(err, StartDownloadL());
				}
			break;
		case EWaitBaudChange:
			{
			RDebug::Printf("[rtlinit] baud change acknowledged: status %d", status);
			// the chip switches right after its Command Complete; our H5 ack of that event must still go out at the
			// old rate (the HCTL queues it before this handler runs), so let the scheduler run for 50 ms
			// (Linux sleeps 10-20 ms) before the host UART moves
			iInitState = EWaitBaudSettle;
			iDelay->After(50000);
			}
			break;
		case EDownloading:
			if (status != 0)
				{
				RDebug::Printf("[rtlinit] fragment %d refused: status %d", data.Length() > 1 ? data[1] : -1, status);
				err = KErrCorrupt;
				break;
				}
			if (iSent >= iImage.Length())
				{
				RDebug::Printf("[rtlinit] download complete: %d bytes in %d fragments; re-reading the version", iImage.Length(), iFragments);
				iInitState = EWaitPatchedVersion;
				TRAP(err, SendReadLocalVersionL());
				}
			else
				{
				TRAP(err, SendNextFragmentL());
				}
			break;
		default:
			break;
		}
	if (err != KErrNone)
		Finish(err);
	}

TInt CRtlInitialisor::ReadFileL(const TDesC& aName, RBuf8& aBuf)
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RFile f;
	TFileName path(KFwPath);
	path.Append(aName);
	User::LeaveIfError(f.Open(fs, path, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	User::LeaveIfError(f.Size(size));
	aBuf.Close();
	aBuf.CreateL(size);
	User::LeaveIfError(f.Read(aBuf));
	CleanupStack::PopAndDestroy(2);		// f, fs
	return size;
	}

void CRtlInitialisor::LoadFirmwareL(TUint8 aRomVersion)
/**
	The Realtek epatch (v1, "Realtech"): header {sig[8], u32 fw_version, u16 num_patches}, then u16 chip_id[n],
	u16 patch_len[n], u32 patch_off[n]. The patch for chip_id == rom_version + 1 is copied, its last 4 bytes replaced
	by fw_version, and the whole config file appended (Linux btrtl rtlbt_parse_firmware / rtl_download_firmware).
*/
	{
	RBuf8 fw, cfg;
	CleanupClosePushL(fw);
	CleanupClosePushL(cfg);
	ReadFileL(KFwFile, fw);
	ReadFileL(KConfigFile, cfg);
	if (fw.Length() < 14 + 4 || fw.Left(8).Compare(_L8("Realtech")) != 0)
		User::Leave(KErrCorrupt);
	const TUint8* p = fw.Ptr();
	const TUint32 fwVersion = p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
	const TInt n = p[12] | (p[13] << 8);
	if (fw.Length() < 14 + n * 8)
		User::Leave(KErrCorrupt);
	// the extension signature + the project id (opcode 0 / len 1) walked backwards from the end
	if (fw.Right(4).Compare(_L8("\x51\x04\xfd\x77")) != 0)
		User::Leave(KErrCorrupt);
	TInt project = -1;
	for (TInt i = fw.Length() - 5; i > 0;)
		{
		const TUint8 op = p[i], len = p[i - 1];
		if (op == 0xff || i - 1 - len < 0)
			break;
		if (op == 0 && len == 1)
			project = p[i - 2];
		i -= 2 + len;
		}
	const TUint16 want = static_cast<TUint16>(aRomVersion + 1);
	TInt sel = -1;
	for (TInt i = 0; i < n; ++i)
		{
		const TUint16 chip = static_cast<TUint16>(p[14 + 2 * i] | (p[15 + 2 * i] << 8));
		if (chip == want)
			sel = i;
		}
	if (sel < 0)
		{
		RDebug::Printf("[rtlinit] no patch for chip id %d in %d entries", want, n);
		User::Leave(KErrNotFound);
		}
	const TInt lenOff = 14 + 2 * n + 2 * sel, offOff = 14 + 4 * n + 4 * sel;
	const TInt patchLen = p[lenOff] | (p[lenOff + 1] << 8);
	const TUint32 patchOff = p[offOff] | (p[offOff + 1] << 8) | (p[offOff + 2] << 16) | (p[offOff + 3] << 24);
	if (patchLen < 4 || patchOff + patchLen > static_cast<TUint32>(fw.Length()))
		User::Leave(KErrCorrupt);
	if (cfg.Length() < 6 || cfg.Left(4).Compare(KRtlConfigMagic) != 0)
		User::Leave(KErrCorrupt);
	RDebug::Printf("[rtlinit] epatch fw_version %08x, %d patches, project %d: patch #%d (chip id %d) %d B @%x + config %d B",
	               fwVersion, n, project, sel + 1, want, patchLen, patchOff, cfg.Length());
	iImage.Close();
	iImage.CreateL(patchLen + cfg.Length());
	iImage.Append(fw.Mid(patchOff, patchLen - 4));
	iImage.Append(static_cast<TUint8>(fwVersion));
	iImage.Append(static_cast<TUint8>(fwVersion >> 8));
	iImage.Append(static_cast<TUint8>(fwVersion >> 16));
	iImage.Append(static_cast<TUint8>(fwVersion >> 24));
	iImage.Append(cfg);
	// the config's UART entry (offset 0x000c, 16 bytes): the device rate code, and bit 2 of byte 12 = RTS/CTS
	iBaudCode = 0;
	iBaudRate = 0;
	iFlowControl = EFalse;
	const TUint8* c = cfg.Ptr();
	for (TInt i = 6; i + 3 <= cfg.Length();)
		{
		const TUint16 off = static_cast<TUint16>(c[i] | (c[i + 1] << 8));
		const TUint8 len = c[i + 2];
		if (i + 3 + len > cfg.Length())
			break;
		if (off == 0x000c && len >= 4)
			{
			iBaudCode = c[i + 3] | (c[i + 4] << 8) | (c[i + 5] << 16) | (c[i + 6] << 24);
			iBaudRate = RateOfCode(iBaudCode);
			iFlowControl = len >= 13 && (c[i + 3 + 12] & 0x04);
			}
		i += 3 + len;
		}
	iFragments = iImage.Length() / KRtlFragment + 1;
	CleanupStack::PopAndDestroy(2);		// cfg, fw
	}

void CRtlInitialisor::BaudSettled()
	{
	if (iInitState != EWaitBaudSettle)
		return;
	TInt err = KErrNotSupported;
	MHctlConfigInterface* configIf = static_cast<MHctlConfigInterface*>(iCoreHci->MchHctlPluginInterface(TUid::Uid(KHctlConfigInterfaceUid)));
	if (configIf)
		err = configIf->MhciUpdateBaudRate(iBaudRate);
	if (err == KErrNone)
		TRAP(err, StartDownloadL());
	if (err != KErrNone)
		Finish(err);
	}

CRtlDelay* CRtlDelay::NewL(CRtlInitialisor& aOwner)
	{
	CRtlDelay* self = new (ELeave) CRtlDelay(aOwner);
	CleanupStack::PushL(self);
	self->ConstructL();
	CActiveScheduler::Add(self);
	CleanupStack::Pop(self);
	return self;
	}

CRtlDelay::CRtlDelay(CRtlInitialisor& aOwner)
  : CTimer(EPriorityStandard), iOwner(aOwner)
	{
	}

void CRtlDelay::RunL()
	{
	iOwner.BaudSettled();
	}

void CRtlInitialisor::StartDownloadL()
	{
	iInitState = EDownloading;
	iSent = 0;
	iFragIndex = 0;
	RDebug::Printf("[rtlinit] downloading %d bytes (%d fragments)", iImage.Length(), iFragments);
	SendNextFragmentL();
	}

void CRtlInitialisor::SendNextFragmentL()
/**
	One 0xfc20 fragment: {index, up to 252 data bytes}. The index runs 0, 1, 2 ... 0x7f, 1, 2 ...; the last fragment
	has bit 7 set (and may be empty when the image is a multiple of 252, as Linux sends it).
*/
	{
	const TInt left = iImage.Length() - iSent;
	const TInt n = left < KRtlFragment ? left : KRtlFragment;
	const TBool last = (iSent + n >= iImage.Length());
	TBuf8<1 + KRtlFragment> frag;
	frag.Append(static_cast<TUint8>(last ? (iFragIndex | 0x80) : iFragIndex));
	frag.Append(iImage.Mid(iSent, n));
	iSent += n;
	iFragIndex = static_cast<TUint8>(iFragIndex == 0x7f ? 1 : iFragIndex + 1);
	SendVendorL(KRtlDownload, frag);
	}
