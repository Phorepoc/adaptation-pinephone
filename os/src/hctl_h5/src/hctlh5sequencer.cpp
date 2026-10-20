// Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
//

/**
 @file
 @internalComponent
*/

#include "hctlh5sequencer.h"

#include "hctlh5FrameQueue.h"
#include "hctlh5.h"
#include "hctlh5frame.h"
#include "hctlh5consts.h"
#include "hctlh5sender.h"

#include "debug.h"

#include <bluetooth/hci/delay.h>


#define MAX_RX_TIMEOUT_ALLOWED			5
/**
Implementation of Class CHCTLH5Sequencer
*/

CHCTLH5Sequencer::CHCTLH5Sequencer(CHCTLH5& aHctlH5, CHCTLH5FrameQueue& aH5FrameQueue):
	iRelRxState(EIdle), 
	iHctlH5(aHctlH5),
	iFrameQueue(aH5FrameQueue)
	{
	LOG_FUNC
	}

CHCTLH5Sequencer* CHCTLH5Sequencer::NewL(CHCTLH5 &aHCTLH5, RBusDevComm& aPort, CHCTLH5FrameQueue& aH5FrameQueue)
	{
	LOG_STATIC_FUNC

	CHCTLH5Sequencer* self=new(ELeave) CHCTLH5Sequencer(aHCTLH5, aH5FrameQueue);
    CleanupStack::PushL(self);
    self->ConstructL(aPort);
    CleanupStack::Pop();
    return self;
	}

void CHCTLH5Sequencer::ConstructL(RBusDevComm& aPort)
    {
	LOG_FUNC

	TCallBack cb = TCallBack(CHCTLH5Sequencer::RxTimeout, this);
	iRxAckTimer = CDelay::NewL(cb, CActive::EPriorityStandard);

	cb = TCallBack(CHCTLH5Sequencer::TxTimeout, this);
	iTxAckTimer = CDelay::NewL(cb, CActive::EPriorityStandard);

    iSender  = CHCTLH5Sender::NewL(*this,aPort);
    }

CHCTLH5Sequencer::~CHCTLH5Sequencer()
	{
	LOG_FUNC

	delete iRxAckTimer;
	delete iTxAckTimer;
	delete iSender;
	}

/**
  	Resets the BCSP sequencer to default state.
*/
void CHCTLH5Sequencer::Reset()
 	{
	LOG_FUNC

 	iSender->Cancel();
 	//reset rx variables
 	iRxAck = 0;
 	iRxSeq = 0;
 	iRxSeqExpected = 0;
 	iRxSeqLast = 0;
 	}
 
/**
  	Called when a peer reset of BCSP is detected
 	Passes the notification up the BCSP stack for handling
*/
void CHCTLH5Sequencer::HandlePeerReset()
 	{
	LOG_FUNC

 	iHctlH5.HandlePeerReset();
 	}
 
void CHCTLH5Sequencer::HandleRx(CRxHctlH5Frame* aFrame)
	{
	LOG_FUNC

	iRelRxState = EReceiving;

	iRxSeqLast = iRxSeq;	
	iRxAck = aFrame->Ack();
	
	if (aFrame->IsReliableProtcolType())
		{
		iRxSeq=aFrame->Sequence();
		}


#ifdef __DEBUG_SEQ_Values__

	LOG1(_L8("\tiRxAck value = %d"),iRxAck);
	LOG1(_L8("\tiRxSeq value = %d"),iRxSeq);
	LOG1(_L8("\tiRxSeqLast value = %d"),iRxSeqLast);
	LOG1(_L8("\tiRxSeqExpected value = %d"),iRxSeqExpected);
	LOG1(_L8("\tiTxAck value = %d"),iTxAck);
#endif

	// Call HandleRxAck() to process the Ack Value conveyed in the received frame header
	HandleRxAck();

	// Check if the received frame is unreliable and on the BCSP Link Establihsment channel
	// If it is then route directly to the iHctlH5.PacketRouter()
	if(!aFrame->IsReliableProtcolType() && aFrame->ProtocolId() == KH5LinkChnl) //Route LE packets
		{
#ifdef __DEBUG_SEQ_VERBOSE__ //Enable flogging for Sequencer
		LOG(_L8("HCTLH5: CHCTLH5Sequencer::HandleRx() Pass LE Packet to LE Entity\n\n"));
#endif
		iHctlH5.PacketRouter();
		return;
		}

	// Otherwise the packet is reliable and passes into this switch statement
	switch (iRelRxState)	//Verify ProtocolId is valid and permissable
		{
		case EIdle: // Fall through.
		case EReceiving:
			{
			if(iRxSeq == iRxSeqExpected && (iHctlH5.CheckIsAckPacket()==EFalse))
				{
				iTxAck = static_cast <TUint8>((iRxSeq+1)%8);
				iRxSeqExpected = iTxAck;
				// OURS (H5): acknowledge at once, as Linux hci_h5 does, and BEFORE the packet goes up the stack --
				// the controller's Command Complete for the Realtek baud-rate command must be acked at the old rate
				// before the initialisor moves the UART; BCSP waited for the 250 ms ack timer (boot 190: no ack, the
				// download never started). The ack rides on the next frame if the sender is busy.
				iHctlH5.TxAckMsg();
				iHctlH5.PacketRouter();	// Forward RxFrame to PacketRouter
				}
			else
				{
				//drop the packet (out of sequence or a pure ack): the timer still acknowledges what we have
				StartTxAckTimer();
				}
			}
			break;

		default:
			break;
		};
	}

void CHCTLH5Sequencer::SendNextPacket()
/**
	This method attempts to send the next frame on the queue.
*/
	{
	LOG_FUNC

	if(SendQueuedFrame() == KErrH5MaxRetries)
		{
		HandlePeerReset();
		}
	}

TUint8 CHCTLH5Sequencer::TxAck() const
	{
	LOG_FUNC

	return iTxAck;
	}

void CHCTLH5Sequencer::HandleRxAck()
	{
	LOG_FUNC

	// An ACK has been received, so reset the Tx timeout counter
	iRxTimeoutCount = 0;
	// Returns ETrue if there are unacknowledged packets remaining
	// in the window
	if(iFrameQueue.AckReceived(iRxAck))
		{
		StartRxAckTimer();
		}
	else
		{
		iRxAckTimer->Cancel();
		}
	}

/*static*/ TInt CHCTLH5Sequencer::RxTimeout(TAny* aThis)
	{
	LOG_STATIC_FUNC
	
	reinterpret_cast<CHCTLH5Sequencer*>(aThis)->HandleRxAckTimeout();
	
	return KErrNone;
	}

void CHCTLH5Sequencer::SendPacket(TDesC8* aFrame, TBool aIsReliable)	
/**
	
	This method writes @param aFrame and starts the RxAckTimer if @param aIsReliable
	is ETrue
*/
	{
	LOG_FUNC

	iSender->Write(*aFrame);	

	if (aIsReliable)
		{
		StartRxAckTimer();
		}
	}

TInt CHCTLH5Sequencer::TxTimeout(TAny* aThis)
	{
	LOG_STATIC_FUNC
	
	reinterpret_cast<CHCTLH5Sequencer*>(aThis)->TxAckMsg();
	
	return KErrNone;
	}

void CHCTLH5Sequencer::TxAckMsg() 
	{
	LOG_FUNC

	iHctlH5.TxAckMsg();
	}

void CHCTLH5Sequencer::StartRxAckTimer()
	{
	LOG_FUNC

	iRxAckTimer->Cancel();
	iRxAckTimer->After(KRxAckTimeout);
	}

void CHCTLH5Sequencer::StartTxAckTimer()
	{
	LOG_FUNC

	if ( !iTxAckTimer->IsActive() )
		{
		iTxAckTimer->After(KRxAckTimeout);
		}
	}

void CHCTLH5Sequencer::HandleRxAckTimeout()
	{
	LOG_FUNC

	// Increment the Rx ACK Timeout counter
	iRxTimeoutCount++;
	iFrameQueue.AckTimeout();
	// While the maximum rx timeout allowed is not reached, rearm the timer
	// waiting for the ACK.
	// If the max is reached, we consider that the peer will not respond and
	// has been poweroff. So, do not rearm the timer and reset the counter.
	// Notice that the BC4 ship implements the same mechanism.
	if (iRxTimeoutCount < MAX_RX_TIMEOUT_ALLOWED)
		{
		StartRxAckTimer();
		}
	else
		{
		iRxTimeoutCount = 0;
		}
	}

void CHCTLH5Sequencer::WakeUp()
/**
	Method made available to trigger the TxStateMachine to do something 
	depending upon its current state
*/
	{
	LOG_FUNC
	if(!iSender->IsActive())
		{
		SendNextPacket();
		}
	}

TInt CHCTLH5Sequencer::SendQueuedFrame()
/**
	Method to get the Next Frame from the frame queue and then write it via 
	the SendPacket method
*/
	{
	LOG_FUNC

	TDesC8* frame = NULL;
	TBool isReliable = EFalse;
	TInt err = iFrameQueue.GetNextFrame(frame,isReliable);
	if (!err)
		{
		SendPacket(frame,isReliable);
		}
	return err;
	}


