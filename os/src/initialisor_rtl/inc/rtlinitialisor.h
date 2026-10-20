// rtlinitialisor.h -- OURS (Bluetooth rung R3, 2026-10-10): the Realtek RTL8723CS controller-initialisation plug-in
// of the 2010 HCIv2 framework. Shape of the TI initialisor (bthci/hci2implementations/initialisors/ti, Nokia, EPL
// v1.0): an ECOM plug-in the Symbian CoreHCI loads from corehci_symbian.ini, driven by the stack through
// MControllerInitialisationInterface, sending vendor commands through the HCI command queue in its pre-reset phase.
// What it does (the Linux btrtl order, docs/phorepoc/research/bluetooth-linux.md 1): Read_Local_Version (expect LMP
// subversion 0x8703 = an unpatched 8723CS), vendor 0xfc61 chip type, 0xfc6d ROM version, pick the patch whose
// chip id = ROM version + 1 from z:\private\101f7989\bluetooth\rtl8723cs_xx_fw.bin, append the config file, switch
// the UART to the config's baud rate (vendor 0xfc17 + the HCTL's MhciUpdateBaudRate), download the image in 252-byte
// 0xfc20 fragments, re-read the version, then let the stack send HCI_Reset. The firmware files are Realtek's,
// redistributed unmodified with their notice (never in deliverables/); the patch is assembled in RAM only.
/**
 @file
 @internalComponent
*/

#ifndef RTLINITIALISOR_H
#define RTLINITIALISOR_H

#include <bluetooth/hci/controllerinitialisationplugin.h>
#include <bluetooth/hci/controllerinitialisationinterface.h>
#include <bluetooth/hcicommandqueueclient.h>

class TReadLocalVersionInfoCompleteEvent;
class TVendorDebugCompleteEvent;
class MCoreHci;

_LIT(KRtlInitialisorPanic, "RtlInitialisor");
enum TRtlInitialisorPanic
	{
	ERtlNullInitialisationObserver = 0,
	ERtlHciCommandQueueOverwritten = 1,
	ERtlInitialisationObserverOverwritten = 2,
	ERtlCoreHciOverwritten = 3,
	ERtlInvalidInitialisorState = 4,
	};

class CRtlInitialisor;

/** A one-shot delay in the stack thread's active scheduler (the H5 ack of the baud-rate command must go out at the
old rate before the UART moves: Linux sleeps 10-20 ms there, we let the scheduler run instead of blocking) */
NONSHARABLE_CLASS(CRtlDelay) : public CTimer
	{
public:
	static CRtlDelay* NewL(CRtlInitialisor& aOwner);
private:
	CRtlDelay(CRtlInitialisor& aOwner);
	void RunL();
	CRtlInitialisor& iOwner;
	};

NONSHARABLE_CLASS(CRtlInitialisor) : public CControllerInitialisationPlugin,
									 public MControllerInitialisationInterface,
									 public MHCICommandQueueClient
	{
	friend class CRtlDelay;
private:
	enum TInitState
		{
		EIdle,
		EWaitLocalVersion,			// Read_Local_Version of the ROM firmware
		EWaitChipType,				// 0xfc61 read of the chip-type register
		EWaitRomVersion,			// 0xfc6d
		EWaitBaudChange,			// 0xfc17
		EWaitBaudSettle,			// the delay before the UART moves to the new rate
		EDownloading,				// 0xfc20 fragments
		EWaitPatchedVersion,		// Read_Local_Version of the patched firmware
		};

public:
	static CRtlInitialisor* NewL();
	virtual ~CRtlInitialisor();
	virtual TAny* Interface(TUid aUid);

private:
	CRtlInitialisor();
	void ConstructL();

	static TInt AsyncCallBackForPreReset(TAny* aInitPlugin);
	static TInt AsyncCallBackForPostReset(TAny* aInitPlugin);
	void DoPreReset();
	void DoPostReset();

	// MControllerInitialisationInterface
	virtual void MciiPreResetCommand();
	virtual void MciiPostResetCommand();
	virtual void MciiSetHCICommandQueue(MHCICommandQueue& aHCICommandQueue);
	virtual void MciiSetControllerInitialisationObserver(MControllerInitialisationObserver& aObserver);
	virtual void MciiSetCoreHci(MCoreHci& aCoreHci);

	// MHCICommandQueueClient
	virtual void MhcqcCommandEventReceived(const THCIEventBase& aEvent, const CHCICommandBase* aRelatedCommand);
	virtual void MhcqcCommandErrored(TInt aErrorCode, const CHCICommandBase* aCommand);

	void HandleLocalVersion(const TReadLocalVersionInfoCompleteEvent& aEvent);
	void HandleVendorComplete(const TVendorDebugCompleteEvent& aEvent);

	void SendReadLocalVersionL();
	void SendVendorL(TUint16 aOpcode, const TDesC8& aParams);
	void SendNextFragmentL();
	void StartDownloadL();
	void BaudSettled();
	void LoadFirmwareL(TUint8 aRomVersion);
	TInt ReadFileL(const TDesC& aName, RBuf8& aBuf);
	void Finish(TInt aError);

private:
	CAsyncCallBack* iAsyncCallBackForPreReset;
	CAsyncCallBack* iAsyncCallBackForPostReset;
	CRtlDelay* iDelay;
	TInitState iInitState;
	MHCICommandQueue* iHCICommandQueue;
	MControllerInitialisationObserver* iControllerInitialisationObserver;
	MCoreHci* iCoreHci;

	TUint16 iLmpSubversion;
	TUint8 iChipType;
	TUint8 iRomVersion;
	RBuf8 iImage;				// the selected patch + fw_version + the whole config file
	TInt iSent;					// bytes of iImage already sent
	TUint8 iFragIndex;			// next 0xfc20 index (wraps 0x7f -> 1)
	TInt iFragments;
	TUint32 iBaudCode;			// the config's 0xfc17 rate code (0 = none)
	TUint32 iBaudRate;			// ... as bits per second for the HCTL
	TBool iFlowControl;
	TUint32 iIniBaud;			// hctl_h5.ini-style option: initialisor_rtl.ini [rtl] baud (0 = keep 115200)
	};

#endif // RTLINITIALISOR_H
