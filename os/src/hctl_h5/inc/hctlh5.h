// hctlh5.h -- OURS (Bluetooth rung R2, 2026-10-10): the H5 (Three-wire UART) HCTL for the PinePhone's RTL8723CS on
// UART1. Forked from the 2010 BCSP HCTL (Nokia, EPL v1.0; hctlbcsp.h): the SLIP framing, the header, the checksum,
// the CRC, the reliable window and the receiver/sender/sequencer are BCSP's unchanged; the H5 differences are the
// packet types (hctlh5consts.h), the 2-byte link messages with a configuration field (linkestablishment.cpp), the
// WAKEUP/WOKEN exchange, GPIO power control through the port's DTR line instead of CSR BCCMDs
// (hctlh5controllermanager.cpp), and the MHctlConfigInterface baud-rate switch the initialisor uses after the
// firmware download. docs/phorepoc/research/bluetooth-linux.md 2, bluetooth-2010.md b.3.
/**
 @file
 @internalComponent
*/

#ifndef HCTLH5_H
#define HCTLH5_H

#include <bluetooth/hci/hctluartbase.h>
#include <bluetooth/hciframelogger.h>
#include "hctlconfiginterface.h"

class CHCTLH5Receiver;
class CLinkEstablishment;
class CHCTLH5Sequencer;
class CHCTLH5FrameQueue;
class CRxHctlH5Frame;
class CHCTLH5ControllerManager;

/**H5 HCI Framing constants for command frames */
static const TInt KHCTLCommandHeaderSize =0;
static const TInt KHCTLCommandTrailerSize=0;

/**H5 HCI Framing constants for ACL data frames */
static const TInt KHCTLAclDataHeaderSize =0;
static const TInt KHCTLAclDataTrailerSize=0;

/**H5 HCI Framing constants for Synchronous data frames */
static const TInt KHCTLSynchronousDataHeaderSize =0;
static const TInt KHCTLSynchronousDataTrailerSize=0;

_LIT(KIniFileName, "hctl_h5");

/**
	This is the class that implements the H5 specific HCTL.
*/
NONSHARABLE_CLASS(CHCTLH5) : public CHCTLUartBase, public MHctlConfigInterface
	{
public:
	static CHCTLH5* NewL();

	void DoConfigL();
	void CanSend(TBool aCanSend);	// H5 opens/closes all channels together
	void QueueReadForNextFrame();
	TInt HandleRx(const TDesC8& aReceivedFrame); // Handles slip decoding and H5 frame decoding
	TInt PacketRouter();
	void TxAckMsg();
	void TxLinkMsg(const TDesC8 &aData);
	void FlogRx();
	TBool CheckIsAckPacket() const;
	void UnChoke();
	void Choke();
	void HandlePeerReset();
	void Reset();
	TBool Muzzled();
	void ResetMuzzled();
	void SetMuzzled();

	// H5 configuration (CLinkEstablishment reports the controller's CONFIG_RSP field)
	void SetPeerConfig(TUint8 aField);
	TBool CrcEnabled() const;

	// controller power (CHCTLH5ControllerManager): the port's DTR signal is the chip's enable + wake pins
	void PowerPulse();
	void PowerOff();

	// a bounded hex trace to the board log (A64_RDEBUG_LOG): link messages, power, the first HCI frames
	void Trace(const char* aTag, const TDesC8& aData);

	~CHCTLH5();

private:
	CHCTLH5();
	void ConstructL();

	TAny* Interface(TUid aUid);

	void ProcessACLData();
	void ProcessEventData();
	void ProcessSynchronousData();

	void DoWriteCommandL(const TDesC8& aData);
	void DoWriteACLDataL(const TDesC8& aData);

	// From CHCTLUartBase
	void PortOpenedL();

	// From MHCTLInterface
	TInt MhiWriteCommand(const TDesC8& aData);
	TInt MhiWriteAclData(const TDesC8& aData);
	TInt MhiWriteSynchronousData(const TDesC8& aData);
	void MhiGetAclDataTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const;
	void MhiGetSynchronousDataTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const;
	void MhiGetCommandTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const;
	void MhiSetChannelObserver(MHCTLChannelObserver& aChannelObserver);
	void MhiSetDataObserver(MHCTLDataObserver& aDataObserver);
	void MhiSetEventObserver(MHCTLEventObserver& aEventObserver);
	void MhiSetControllerStateObserver(MControllerStateObserver& aControllerStateObserver);
	void MhiSetQdpPluginInterfaceFinder(MQdpPluginInterfaceFinder& aQdpPluginInterfaceFinder);

	// From MHardResetInitiator
	void MhriStartHardReset();

	// From MHctlConfigInterface (the initialisor, after the vendor baud-rate command)
	TInt MhciUpdateBaudRate(TUint32 aBaudRate);

private:
	TBool iMuzzled;
	TBool iCanSend;
	//When H5 is choked, it means we hasn't established a link with the controller yet.
	//Only UART link management packets are allowed to pass through.
	TBool iChoked;

	CHCTLH5Receiver* iReceiver;
	CHCTLH5Sequencer* iSequencer;
	CLinkEstablishment* iLinkEstablishment;
	MControllerStateObserver* iControllerStateObserver;
	MQdpPluginInterfaceFinder* iQdpPluginInterfaceFinder;
	CHCTLH5FrameQueue* iFrameQueue;
	CRxHctlH5Frame* iReceivedFrame;
	TInt iCommUnit;
	TUint8 iAck;
	CHCTLH5ControllerManager* iControllerMan;

	TBool iCrcWanted;		// hctl_h5.ini [hctl] crc= : put a data-integrity check on our reliable frames when the
							// controller agrees (default off: Linux never sends one, and the chip is known to work so)
	TBool iUseCrc;			// ... and the controller agreed (CONFIG_RSP field bit 4)
	TUint8 iPeerWindow;		// the controller's sliding window (CONFIG_RSP field bits 0-2)
	TInt iTraceLeft;		// HCI frames still to trace this link
	DECLARE_HCI_LOGGER
	};

#endif // HCTLH5_H
