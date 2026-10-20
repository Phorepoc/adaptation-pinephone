// hctlh5.cpp -- OURS (Bluetooth rung R2, 2026-10-10): the H5 HCTL, forked from the 2010 BCSP HCTL (Nokia, EPL v1.0;
// hctlbcsp.cpp). See hctlh5.h for what differs.
/**
 @file
 @internalComponent
*/

#include "hctlh5.h"
#include "hctlh5FrameQueue.h"
#include "h5utils.h"
#include "hctlh5frame.h"
#include "hctlh5consts.h"
#include "linkestablishment.h"
#include "hctlh5receiver.h"
#include "hctlh5sequencer.h"
#include "hctlh5controllermanager.h"
#include "debug.h"

#include <e32debug.h>
#include <bluetooth/hci/hciutil.h>
#include <bluetooth/hci/hcierrors.h>
#include <bluetooth/hci/hcievents.h>
#include <bluetooth/hci/hctlchannelobserver.h>
#include <bluetooth/hci/hctldataobserver.h>
#include <bluetooth/hci/hctleventobserver.h>

const TInt KH5TraceFramesPerLink = 200;		// HCI frames traced per link (re-armed by every Reset; the firmware
											// download alone is 57 + the stack's init ~25)

/**
	Implementation of Class CHCTLH5
*/
CHCTLH5::CHCTLH5()
	{
	LOG_FUNC
	}

CHCTLH5::~CHCTLH5()
	{
	LOG_FUNC
	HCI_LOG_UNLOAD(this);
	delete iLinkEstablishment;
	delete iReceiver;
	delete iSequencer;
	delete iFrameQueue;
	delete iReceivedFrame;
	delete iControllerMan;
	}

CHCTLH5* CHCTLH5::NewL()
	{
	LOG_STATIC_FUNC
	CHCTLH5* self=new(ELeave) CHCTLH5;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CHCTLH5::ConstructL()
	{
	LOG_FUNC
	HCI_LOG_LOADL(this, KHCILoggerDatalinkTypeH1);	// logged at the HCI layer, above the H5 framing
	// our own ini options (the base class reads port / baud_rate / pwr_ctrl_mode from the same file)
		{
		CHciUtil* hciUtil = CHciUtil::NewL(KIniFileName);
		CleanupStack::PushL(hciUtil);
		hciUtil->OpenIniFileL();
		_LIT(KSection, "hctl");
		_LIT(KVarCrc, "crc");
		TUint crc = 0;
		TRAPD(err, crc = hciUtil->GetValueFromFileL(KSection, KVarCrc));
		iCrcWanted = (err == KErrNone && crc != 0);
		CleanupStack::PopAndDestroy(hciUtil);
		}
	BaseConstructL(KIniFileName);
	iControllerMan = CHCTLH5ControllerManager::NewL(*this);
	}

TAny* CHCTLH5::Interface(TUid aUid)
	{
	LOG_FUNC
	TAny* ret = NULL;
	switch(aUid.iUid)
		{
		case KHCTLInterfaceUid:
			ret = reinterpret_cast<TAny*>(static_cast<MHCTLInterface*>(this));
			break;
		case KHCTLPowerInterfaceUid:
			ret = reinterpret_cast<TAny*>(static_cast<MHCTLPowerInterface*>(iControllerMan));
			break;
		case KHCHardResetUid:
			ret = reinterpret_cast<TAny*>(static_cast<MHardResetInitiator*>(this));
			break;
		case KHctlConfigInterfaceUid:
			ret = reinterpret_cast<TAny*>(static_cast<MHctlConfigInterface*>(this));
			break;
		default:
			break;
		};
	return ret;
	}

void CHCTLH5::DoConfigL()
/**
	The TCommConfigV01 settings of the H5 link: 8 data bits, even parity, one stop bit ("devices always start with
	these fixed parameters", Linux hci_h5), 115200, the SLIP delimiter 0xC0 as the read terminator so the serial LDD
	hands frames over whole, and both RTS and DTR left to us: our UART1 PDD drives the chip's enable + wake pins from
	DTR, and the LDD would otherwise assert it at every Start.
	@leave method will leave if Port().SetConfig() fails
*/
	{
	LOG_FUNC
	Port().ResetBuffers();
	TCommConfig conf;
	Port().Config(conf);
	TCommConfigV01& config = conf(); //Get reference to TCommConfig iConfig
	config.iRate = EBps115200;
	config.iDataBits = EData8;
	config.iStopBits = EStop1;
	config.iTerminatorCount = 1;
	config.iTerminator[0] = 0xc0;
	config.iParity = EParityEven;
	config.iParityError = KConfigParityErrorFail;
	config.iHandshake = KConfigFreeRTS | KConfigFreeDTR;
	config.iXonChar = 0;
	config.iXoffChar = 0;
	LEAVEIFERRORL(Port().SetConfig(conf));
	// the LDD's receive ring (2 KB by default) must hold more than one largest SLIP frame
	LEAVEIFERRORL(Port().SetReceiveBufferLength(2 * KHCTLRecvBufSize));
	}

TInt CHCTLH5::MhiWriteAclData(const TDesC8& aData)
	{
	LOG_FUNC
	Trace("tx acl", aData);
	iFrameQueue->AddReliableFrame(aData, KH5ACLDataChnl);
	HCI_LOG_FRAME(this, KHCILoggerHostToController | KHCILoggerACLDataFrame, aData);
	return KErrNone;
	}

TInt CHCTLH5::MhiWriteSynchronousData(const TDesC8& aData)
	{
	LOG_FUNC
	iFrameQueue->AddUnreliableFrame(aData, KH5SynchronousDataChnl, iSequencer->TxAck(), EFalse);
	HCI_LOG_FRAME(this, KHCILoggerHostToController | KHCILoggerSynchronousDataFrame, aData);
	return KErrNone;
	}

TInt CHCTLH5::MhiWriteCommand(const TDesC8& aData)
	{
	LOG_FUNC
	Trace("tx cmd", aData);
	TRAPD(err,DoWriteCommandL(aData));
	if(err!=KErrNone)
		{
		return KErrH5WriteCommandDataFailed;
		}
	else
		{
		HCI_LOG_FRAME(this, KHCILoggerHostToController | KHCILoggerCommandOrEvent, aData);
		return err;
		}
	}

void CHCTLH5::MhiSetQdpPluginInterfaceFinder(MQdpPluginInterfaceFinder& aQdpPluginInterfaceFinder)
	{
	iQdpPluginInterfaceFinder = &aQdpPluginInterfaceFinder;
	}

void CHCTLH5::MhriStartHardReset()
	{
	iControllerMan->HardReset();
	}

void CHCTLH5::MhiGetAclDataTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const
	{
	aHeaderSize = KHCTLAclDataHeaderSize;
	aTrailerSize = KHCTLAclDataTrailerSize;
	}

void CHCTLH5::MhiGetSynchronousDataTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const
	{
	aHeaderSize = KHCTLSynchronousDataHeaderSize;
	aTrailerSize = KHCTLSynchronousDataTrailerSize;
	}

void CHCTLH5::MhiGetCommandTransportOverhead(TUint& aHeaderSize, TUint& aTrailerSize) const
	{
	aHeaderSize = KHCTLCommandHeaderSize;
	aTrailerSize = KHCTLCommandTrailerSize;
	}

TInt CHCTLH5::MhciUpdateBaudRate(TUint32 aBaudRate)
/**
	The initialisor's baud-rate switch, after the controller has acknowledged its vendor "set baud rate" command.
	Any rate goes through TBps EBpsSpecial + iSpecialRate = the rate itself (our UART1 PDD's convention; the 2010
	SetPortBaudRateL knows only four fixed specials). Above 115200 the chip's config also turns on RTS/CTS
	hardware flow control: KConfigObeyCTS makes our PDD enable the UART's auto flow control.
*/
	{
	LOG_FUNC
	TCommConfig conf;
	Port().Config(conf);
	TCommConfigV01& config = conf();
	config.iRate = EBpsSpecial;
	config.iSpecialRate = aBaudRate;
	config.iHandshake = KConfigFreeDTR | (aBaudRate > 115200 ? KConfigObeyCTS : KConfigFreeRTS);
	TInt r = Port().SetConfig(conf);
	RDebug::Printf("[hctl_h5] baud -> %u, flow %s: %d", aBaudRate, aBaudRate > 115200 ? "RTS/CTS" : "none", r);
	return r;
	}

void CHCTLH5::DoWriteCommandL(const TDesC8 &aData)
/**
	Method called from MhiWriteCommand
	@param aData
	aData is the formatted Command payload from the linkmanager
*/
	{
	LOG_FUNC
#ifdef __DEBUG_FLOG_STACK_TX_
	LOG(_L8("STACK TX"));
	LOGHEXDESC(aData);
#endif
	User::LeaveIfError(iFrameQueue->AddReliableFrame(aData, KH5CommandChnl));
	}

/**
Process Received data in to the format expected by the interface to the bottom of the stack
*/
void CHCTLH5::ProcessACLData()
/**
	Passes ACL data to the data observer.
*/
	{
	LOG_FUNC
	__ASSERT_DEBUG(iReceivedFrame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
	Trace("rx acl", iReceivedFrame->Payload());
	HCI_LOG_FRAME(this, KHCILoggerControllerToHost | KHCILoggerACLDataFrame, iReceivedFrame->Payload());
	iDataObserver->MhdoProcessAclData(iReceivedFrame->Payload());
	}

void CHCTLH5::ProcessEventData()
/**
	Passes event data to the event observer.
*/
	{
	LOG_FUNC
	__ASSERT_DEBUG(iReceivedFrame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
	Trace("rx evt", iReceivedFrame->Payload());
	HCI_LOG_FRAME(this, KHCILoggerControllerToHost | KHCILoggerCommandOrEvent, iReceivedFrame->Payload());
	iEventObserver->MheoProcessEvent(iReceivedFrame->Payload());
	}

void CHCTLH5::ProcessSynchronousData()
	{
	LOG_FUNC
	__ASSERT_DEBUG(iReceivedFrame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
	HCI_LOG_FRAME(this, KHCILoggerControllerToHost | KHCILoggerSynchronousDataFrame, iReceivedFrame->Payload());
	iDataObserver->MhdoProcessSynchronousData(iReceivedFrame->Payload());
	}

TInt CHCTLH5::PacketRouter()
/**
	Method called by CHCTLH5Receiver after packet integrity checks have been performed
	This routes packets according to the H5 packet type of the payload to the appropriate interface to the top of
	the HCI and thus to the link manager
*/
	{
	LOG_FUNC
	TInt err = KErrNone;
	__ASSERT_DEBUG(iReceivedFrame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));

	switch (iReceivedFrame->ProtocolId())
		{
		case KH5EventChnl:
			{
			LOG(_L8("HCTLH5: Event Packet Received..."));
			ProcessEventData();
			}
			break;
		case KH5ACLDataChnl:
			{
			LOG(_L8("HCTLH5: ACL Data Packet Received..."));
			ProcessACLData();
			}
			break;
		case KH5SynchronousDataChnl:
			{
			LOG(_L8("HCTLH5: Synchronous Data Packet Received..."));
			ProcessSynchronousData();
			}
			break;
		case KH5LinkChnl:
			{
			LOG(_L8("HCTLH5: Link Est Packet Received..."));
			__ASSERT_DEBUG(iLinkEstablishment!=NULL, PANIC(KH5PanicCat, EBadLinkPointer));
			TRAP(err,iLinkEstablishment->ProcessLinkMsg(iReceivedFrame->Payload()));
			}
			break;
		default:
			// a command (type 1) or vendor (14) packet from the controller: nothing above us takes it
			Trace("rx dropped (type)", iReceivedFrame->Payload());
			break;
		};
	return err;
	}

void CHCTLH5::QueueReadForNextFrame()
	{
	LOG_FUNC
	iReceiver->QueueReadForNextFrame();
	}

void CHCTLH5::CanSend(TBool aCanSend)
/**
	For H5 this is always taken to be all channels or no channels
*/
	{
	LOG_FUNC
	iCanSend = aCanSend;

	// We can only open the channel after unchoked and iChannelObserver has been set
	if(!iChoked && iChannelObserver)
		{
		if(iCanSend)
			{
			iChannelObserver->MhcoChannelOpen(KHCITransportAllChannels);
			}
		else
			{
			iChannelObserver->MhcoChannelClosed(KHCITransportAllChannels);
			}
		}
	}

void CHCTLH5::TxAckMsg()
/**
	Method to send an H5 pure acknowledgement
	This is only called when the Ack timer fires i.e. a certain amount of time has
	passed without there being an outgoing H5 frame to convey acknowledgement of received
	reliable frames
*/
	{
	LOG_FUNC
	iFrameQueue->AddUnreliableFrame(KH5AckChnl, iSequencer->TxAck(), EFalse);
	}

void CHCTLH5::TxLinkMsg(const TDesC8& aData)
/**
	Queues an H5 link-control message (unreliable, type 15, never with a data integrity check: Linux hci_h5 and
	the chip's own SYNC/CONFIG frames carry none)
*/
	{
	LOG_FUNC
	Trace("tx link", aData);
	iFrameQueue->AddUnreliableFrame(aData, KH5LinkChnl, 0, EFalse);
	}

TInt CHCTLH5::HandleRx(const TDesC8 &aReceivedFrame)
/**
	This is a method called from the CHCTLH5Receiver::ProcessDataL()
	with the parameter
	@param const TDesC8 &aReceivedFrame
	@return err

	This method attempts to slip decode the received frame
	Then if that is successful the remainder of the H5 frame integrity checks are performed
	using the iReceivedFrame->IsValid() method
	Assuming that this all works without errors control is then passed to the sequencer
	via the iSequencer->HandleRx(iReceivedFrame) call
	and then the iReceivedFrame->Reset() method is called
*/
	{
	LOG_FUNC
	TInt rerr = iReceivedFrame->SlipDecodeFrame(aReceivedFrame);
	if (rerr == KErrNone)
		{
		// We've succeeded in decoding the Slip Frame into a CRxHctlH5Frame
		__ASSERT_DEBUG(iReceivedFrame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
		if (iReceivedFrame->ProtocolId() == KH5LinkChnl)
			Trace("rx link", iReceivedFrame->Payload());
#ifdef __DEBUG_RxDecodedFrame__
		FlogRx();
#endif
		iSequencer->HandleRx(iReceivedFrame);
		iReceivedFrame->Reset();
		}
	else if (aReceivedFrame.Length() > 1)
		{
		Trace("rx bad frame", aReceivedFrame);
		}
	return rerr;
	}

TBool CHCTLH5::CheckIsAckPacket() const
/**
	This is a relatively simple method that examines the ProtocolId of the Received Frame
	and attempts to match it to the KH5AckChnl constant and then returns a TBool depending
	on whether or not these match
*/
	{
	LOG_FUNC
	return (iReceivedFrame->ProtocolId() == KH5AckChnl);
	}

#ifdef __DEBUG_RxDecodedFrame__
void CHCTLH5::FlogRx()
	{
	LOG(_L8("RxFrame"));
	LOG1(_L8("Protocol type reliable(1) unreliable (0) = %d\n"),iReceivedFrame->IsReliableProtcolType());
	LOG1(_L8("Ack value = %d\n"),iReceivedFrame->Ack());
	LOG1(_L8("Seq value = %d\n"),iReceivedFrame->Sequence());
	LOG1(_L8("Packet type = %d\n"),iReceivedFrame->ProtocolId());
	LOG1(_L8("Payload Length = %d\n"),iReceivedFrame->PayloadLength());
	LOG1(_L8("Checksum value = %d\n"),iReceivedFrame->CheckSum());
	LOG1(_L8("Header bytes = %d\n"),iReceivedFrame->FlagsField());
	LOG(_L8("Payload..."));
	LOGHEXDESC(iReceivedFrame->Payload());
	}
#endif

/**
	See comments on iChoked
*/
void CHCTLH5::Choke()
	{
	LOG_FUNC
	iChoked = ETrue;
	CanSend(iCanSend);
	}

/**
	See comments on iChoked
*/
void CHCTLH5::UnChoke()
	{
	LOG_FUNC
	iChoked = EFalse;
	RDebug::Printf("[hctl_h5] link ACTIVE (peer window %d, crc %s)", iPeerWindow, iUseCrc ? "on" : "off");
	if(iControllerMan->H5LinkEstablished())
		{
		CanSend(iCanSend);
		}
	}

TBool CHCTLH5::Muzzled()
	{
	return iMuzzled;
	}

void CHCTLH5::ResetMuzzled()
	{
	iMuzzled = EFalse;
	}

void CHCTLH5::SetMuzzled()
	{
	iMuzzled = ETrue;
	}

void CHCTLH5::SetPeerConfig(TUint8 aField)
/**
	The controller's CONFIG_RSP configuration field: its sliding window (the controller's value wins) and whether
	it agreed to the data integrity check we asked for.
*/
	{
	iPeerWindow = static_cast<TUint8>(aField & KH5ConfigWindowMask);
	iUseCrc = iCrcWanted && (aField & KH5ConfigCrcMask);
	RDebug::Printf("[hctl_h5] CONFIG_RSP field %02x: window %d, crc %s%s", aField, iPeerWindow,
	               (aField & KH5ConfigCrcMask) ? "granted" : "refused", iUseCrc ? " (in use)" : "");
	if (iPeerWindow && iPeerWindow < KH5ReliableWindowSize)
		{
		// our reliable window is sized at construction; a smaller grant would need it rebuilt here (not seen on the RTL8723CS)
		RDebug::Printf("[hctl_h5] WARNING: peer window %d < ours %d", iPeerWindow, KH5ReliableWindowSize);
		}
	}

TBool CHCTLH5::CrcEnabled() const
	{
	return iUseCrc;
	}

void CHCTLH5::PowerPulse()
/**
	The chip's power-on sequence through the port's DTR line (= BT enable PL4 + device-wake PH6 in our UART1 PDD):
	off for 100 ms, then on and 500 ms to wake up (Linux hci_h5 rtl). Synchronous: it runs in the stack thread
	at construction, power-on and hard reset only.
*/
	{
	LOG_FUNC
	Port().SetSignals(0, KSignalDTR);
	User::After(KH5PowerOffHoldTime);
	Port().SetSignals(KSignalDTR, 0);
	User::After(KH5PowerOnSettleTime);
	RDebug::Printf("[hctl_h5] controller powered (signals %x)", Port().Signals());
	}

void CHCTLH5::PowerOff()
	{
	LOG_FUNC
	Port().SetSignals(0, KSignalDTR);
	RDebug::Printf("[hctl_h5] controller off");
	}

void CHCTLH5::Trace(const char* aTag, const TDesC8& aData)
	{
	if (iTraceLeft <= 0)
		return;
	--iTraceLeft;
	TBuf8<80> hex;
	const TInt n = aData.Length() < 24 ? aData.Length() : 24;
	for (TInt i = 0; i < n; ++i)
		hex.AppendFormat(_L8("%02x "), aData[i]);
	RDebug::Printf("[hctl_h5] %s (%d): %S%s", aTag, aData.Length(), &hex, aData.Length() > n ? "..." : "");
	}

/**
	Handle a reset of the H5 transport.
	1) Reset the H5 layer
	2) Inform the stack of a HCI error
	3) Stack will reinitialise with a "reset" command etc
*/
void CHCTLH5::HandlePeerReset()
	{
	LOG_FUNC
	RDebug::Printf("[hctl_h5] peer reset%s", iControllerMan->ExpectedControllerReset() ? " (expected)" : "");
	// If a poweroff is requested, the next reset will lead to a reset of the link
	// and will set the state to shy sending a sync frame every 250ms
	// So, Muzzle the host to avoid sending this command
	if(iControllerMan->PowerOffRequested())
		{
		iMuzzled = ETrue;
		}

	Reset(); //reset H5

	if(!iControllerMan->ExpectedControllerReset() && !iControllerMan->PowerOffRequested())
		{
		// Inform stack of hardware error
		TUint8 errReason = static_cast<TUint8>(EInvalidHCIParameter);
		TBuf8<3> buf(3);
		buf[0]= EHardwareErrorEvent; //error code
		buf[1] = 1; //length
		buf[2] = errReason; //parameter
		iEventObserver->MheoProcessEvent(buf);
		}
	}

/**
	Reset the H5 layer
*/
void CHCTLH5::Reset()
	{
	LOG_FUNC
	Choke();	//Disconnected
	iTraceLeft = KH5TraceFramesPerLink;
	iUseCrc = EFalse;
	//Reset the link establishment state machine
	iLinkEstablishment->Reset();
	//Reset and clear transmit queues
	iFrameQueue->Reset();
	//Reset the sequencer
	iSequencer->Reset();
	}

void CHCTLH5::PortOpenedL()
	{
	LOG_FUNC
	__ASSERT_DEBUG(Port().Handle(), PANIC(KH5PanicCat, EPortNotOpen));
	DoConfigL();
	iChoked = ETrue;
	iTraceLeft = KH5TraceFramesPerLink;
	// power the chip before the link establishment below starts sending SYNC
	PowerPulse();
	iFrameQueue = CHCTLH5FrameQueue::NewL(*this);
	// Creation of the sequencer creates the sender Active Object.
	// The sender Active Object must be added to the Active Scheduler before
	// the receiver Active Object so that it gets preferential treatment. It
	// is reported that otherwise the response from a command can come in
	// before the sending client is told that the send has completed!
	iSequencer = CHCTLH5Sequencer::NewL(*this, Port(), *iFrameQueue);
	iReceiver=CHCTLH5Receiver::NewL(*this, Port());
	iFrameQueue->SetSequencer(*iSequencer);
	iLinkEstablishment= CLinkEstablishment::NewL(*this);
	iReceivedFrame = CRxHctlH5Frame::NewL();
	}

void CHCTLH5::MhiSetDataObserver(MHCTLDataObserver& aDataObserver)
	{
	iDataObserver = &aDataObserver;
	}

void CHCTLH5::MhiSetEventObserver(MHCTLEventObserver& aEventObserver)
	{
	iEventObserver = &aEventObserver;
	}

void CHCTLH5::MhiSetChannelObserver(MHCTLChannelObserver& aChannelObserver)
	{
	iChannelObserver = &aChannelObserver;
	//give the stack an initial kick
	CanSend(iCanSend);
	}

void CHCTLH5::MhiSetControllerStateObserver(MControllerStateObserver& aControllerStateObserver)
	{
	iControllerStateObserver = &aControllerStateObserver;
	iControllerMan->SetObserver(aControllerStateObserver);
	iControllerMan->Start();
	}
