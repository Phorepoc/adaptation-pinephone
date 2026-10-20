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

#include "hctlh5FrameQueue.h"

#include "hctlh5.h"
#include "hctlh5consts.h"
#include "hctlh5sequencer.h"
#include "h5utils.h"
#include "debug.h"
#include "hctlh5frame.h"

CHCTLH5Window::CHCTLH5Window(TInt aWindowSize, TInt aNumberOfSeqIds)
  : iEndOfWindow(aWindowSize - 1),
    iWindowSize(aWindowSize),
    iNumberOfSeqIds(aNumberOfSeqIds),
	iFrameQue(_FOFF(CTxHctlH5Frame,iLink))
	{
	LOG_FUNC
	}

CHCTLH5Window* CHCTLH5Window::NewL(TInt aWindowSize, TInt aFrameSize, TInt aNumberOfSeqIds)
	{
	LOG_STATIC_FUNC

	CHCTLH5Window *self = new (ELeave) CHCTLH5Window(aWindowSize, aNumberOfSeqIds);
	CleanupStack::PushL(self);
	self->ConstructL(aFrameSize);
	CleanupStack::Pop(self);
	return self;
	}

void CHCTLH5Window::ConstructL(TInt aFrameSize)
	{
	LOG_FUNC

	for(TInt i=0;i<iWindowSize;i++)
		{
		CTxHctlH5Frame *frame = CTxHctlH5Frame::NewL(aFrameSize);
		frame->Reset();
		iFrameQue.AddLast(*frame);

#ifdef _DEBUG_WINDOW
		LOG1(_L8("frame added 0x%08x"),frame);
#endif
		}
#ifdef _DEBUG_WINDOW
	TraceWindowVars();
	TraceQueue();
#endif
	}

CHCTLH5Window::~CHCTLH5Window()
	{
	LOG_FUNC

	for (TInt i=0;i<iWindowSize;i++)
		{
		CTxHctlH5Frame* frame = iFrameQue.First();
		iFrameQue.Remove(*frame);
		delete frame;
		}
	}

TInt CHCTLH5Window::StartOfWindow() const
	{
	LOG_FUNC
	 
	return iStartOfWindow; 
	}
	
TInt CHCTLH5Window::ToSeqId(TInt aIndex) const
	{ 
	LOG_FUNC

	return (aIndex + iStartOfWindow)%iNumberOfSeqIds; 
	}
	
TInt CHCTLH5Window::ToWindowIndex(TInt aSeqId) const 
	{
	LOG_FUNC
	
	TInt index = aSeqId - iStartOfWindow;
	if ( index < 0 )
		{
		index += iNumberOfSeqIds;
		}
	return index; 
	}
	
void CHCTLH5Window::IncrementSeqId(TInt &aSeqId) const
	{ 
	aSeqId++; 
	aSeqId %= iNumberOfSeqIds;
	}


CTxHctlH5Frame* CHCTLH5Window::GetFrame(TInt aIndex)
/**
	Method to retrieve a frame for transmission
*/
	{
	LOG_FUNC

	__TEST_INVARIANT;

	TInt index = ToWindowIndex(aIndex);
	__ASSERT_DEBUG((index>=0) && (index<iWindowSize), PANIC(KH5PanicCat, EBadH5Frame));

	TSglQueIter<CTxHctlH5Frame> iter(iFrameQue);
	CTxHctlH5Frame* frame = iter++;

	while ( index > 0 )
		{
		frame = iter++;
		index--;
		}

#ifdef _DEBUG_WINDOW
	LOG2(_L8("frame got 0x%08x (index:%d)"), frame, aIndex);
#endif

	return frame;
	}

TInt CHCTLH5Window::FreeFrames()
/**
	Method to free up space in transmit queue for sending more BCSP frames
*/
	{
	LOG_FUNC

	__TEST_INVARIANT;

	return ToWindowIndex(iEndOfWindow) - ToWindowIndex(iWritePosition);
	}

CTxHctlH5Frame *CHCTLH5Window::WriteFrame()
/**
	Method to Write a frame to the frame queue
*/	
	{
	LOG_FUNC

	CTxHctlH5Frame *frame = NULL;

	__TEST_INVARIANT;

	if ( iWritePosition != iEndOfWindow )
		{
		frame = GetFrame(iWritePosition);
		
		if(frame)
			{
			IncrementSeqId(iWritePosition);
			// Make sure we're not returning a valid frame to be overwritten
			__ASSERT_DEBUG(!frame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
			}
		else
			{
	#ifdef _DEBUG_WINDOW
	FTRACE(FPrintBCSP(_L("Bcsp frame queue blocked (panic now deprecated)")));
	#endif
			}

#ifdef _DEBUG_WINDOW
		LOG(_L8("Post Write"));
		TraceWindowVars();
		TraceQueue();
#endif
		}

	__TEST_INVARIANT;

	return frame;
	}

CTxHctlH5Frame *CHCTLH5Window::ReadFrame()
/**
	Method to read a frame from the Transmit Queue

	@return CTxHctlH5Frame * frame
*/	
	{
	LOG_FUNC

	CTxHctlH5Frame *frame = NULL;

	__TEST_INVARIANT;

	if ( iReadPosition != iWritePosition )
		{
		frame = GetFrame(iReadPosition);
		frame->SetSequence(static_cast<TUint8>(iReadPosition));

		if (iReadPosition == iHighReadPosition)
			{
			IncrementSeqId(iHighReadPosition);
			}
		IncrementSeqId(iReadPosition);

#ifdef _DEBUG_WINDOW
		LOG(_L8("Post Read"));
		TraceWindowVars();
		TraceQueue();
#endif
		// Make sure we're returning a valid frame to be used
		__ASSERT_DEBUG(frame->IsValid(), PANIC(KH5PanicCat, EBadH5Frame));
		}

	__TEST_INVARIANT;

	return frame;
}

TBool CHCTLH5Window::Acknowledged(TInt aAckNo)
/**
	Method to handle TxFrame acknowledgement
	Remove the frame that is acknowledged
	@param current Rx Ack value, KErrTimedOut if ack timed out
*/
	{
	LOG_FUNC

	TBool framesRemainingInWindow = EFalse;

#ifdef _DEBUG_WINDOW
	LOG(_L8("Pre Ack"));
	TraceWindowVars();
#endif

	__TEST_INVARIANT;

	const TBool resetWindow = ((aAckNo >= 0) && (ToWindowIndex(aAckNo) > ToWindowIndex(iHighReadPosition))) ? ETrue : EFalse;

	if (resetWindow)
		{
#ifdef _DEBUG_WINDOW
		LOG1(_L8("Ack for unsent packet (%d), resetting entire window!!!"), aAckNo);
#endif
		// reset frames
		TSglQueIter<CTxHctlH5Frame> iter(iFrameQue);
		CTxHctlH5Frame *frame = iter++;

		while (iter)
			{
			frame->Reset();
			frame = iter++;
			}

		//reset window variables
		iStartOfWindow = aAckNo;
		iStartOfWindow %= iNumberOfSeqIds;

		iEndOfWindow  = iStartOfWindow + iWindowSize-1;
		iEndOfWindow %= iNumberOfSeqIds;

		iWritePosition = iHighReadPosition = iReadPosition = iStartOfWindow;

		framesRemainingInWindow = ETrue;
		}
	else if (iReadPosition != iStartOfWindow) // frames waiting to be acknowledged
		{
		LOG1(_L("Ack(%d)"), aAckNo);

		if ( aAckNo >= 0 )
			{
			aAckNo %= iNumberOfSeqIds; // Make sure we wrap around at the end of the window
			LOG1(_L8("Ack(%d)"), aAckNo);

			// TInt  readIndex = ackWindowIndex;
			TBool found = EFalse;
			TInt i = iStartOfWindow;

			while(!found && (i != iEndOfWindow))
				{
				if((i == aAckNo))
					{
					found = ETrue;
					}
				else if(!found)
					{
					// This frame is before the ackWindowIndex therefore we should reset it
					CTxHctlH5Frame* frame = iFrameQue.First();
					frame->Reset();
					iFrameQue.Remove(*frame);
					iFrameQue.AddLast(*frame);
					}
				IncrementSeqId(i);
				}

			iStartOfWindow = aAckNo;
			iStartOfWindow %= iNumberOfSeqIds;

			iEndOfWindow = iStartOfWindow + iWindowSize - 1;
			iEndOfWindow %= iNumberOfSeqIds;
			}

		// We've just had an acknowledgement so we need to resend from the start of the buffer
		iReadPosition = iStartOfWindow;

		// We've just rationalised the window. The write position can remain where it was.
		framesRemainingInWindow = ETrue;
		}

#ifdef _DEBUG_WINDOW
	LOG(_L("Post Ack"));
	TraceWindowVars();
	TraceQueue();
#endif

	__TEST_INVARIANT;

	return framesRemainingInWindow;
	}

void CHCTLH5Window::__DbgTestInvariant(void) const
/**
	Debug method
*/
	{
	// Check that the class is internally consistent
	__ASSERT_DEBUG(iNumberOfSeqIds>0,                                              User::Invariant());
	__ASSERT_DEBUG((iWindowSize>0) && (iWindowSize <= iNumberOfSeqIds),            User::Invariant());

	__ASSERT_DEBUG((iStartOfWindow>=0) && (iStartOfWindow<=iNumberOfSeqIds),       User::Invariant());
	__ASSERT_DEBUG((iEndOfWindow>=0)   && (iEndOfWindow<=iNumberOfSeqIds),         User::Invariant());
	__ASSERT_DEBUG((iReadPosition>=0 ) && (iReadPosition<=iNumberOfSeqIds),        User::Invariant());
	__ASSERT_DEBUG((iHighReadPosition>=0 ) && (iHighReadPosition<=iNumberOfSeqIds),User::Invariant());
	__ASSERT_DEBUG((iWritePosition>=0) && (iWritePosition<=iNumberOfSeqIds),       User::Invariant());

	TInt winSize = iEndOfWindow - iStartOfWindow + 1;
	if ( winSize < 0 )
		{
		winSize += iNumberOfSeqIds;
		}
	__ASSERT_DEBUG(winSize == iWindowSize,   User::Invariant());
	if ( iWritePosition != iEndOfWindow )
		{
		__ASSERT_DEBUG(ToWindowIndex(iReadPosition) <= ToWindowIndex(iHighReadPosition),  User::Invariant());
		__ASSERT_DEBUG(ToWindowIndex(iHighReadPosition) <= ToWindowIndex(iWritePosition), User::Invariant());
		}
	}

#ifdef _DEBUG_WINDOW
void CHCTLH5Window::TraceWindowVars() const
/**
	Debug method
*/
	{
	LOG_FUNC
	RProcess process;
	RThread thread;
	TName pName = process.Name();
	TName tName = thread.Name();
	LOG1(_L8("Process : %S"), &pName);
	LOG1(_L8("Thread : %S"), &tName);
	LOG6(_L8("this : 0x%08x    SOW : %d, RP : %d, HRP: %d, WP : %d, EOW :%d"),this, iStartOfWindow, iReadPosition, iHighReadPosition, iWritePosition, iEndOfWindow);
	}

void CHCTLH5Window::TraceQueue()
/**
	Debug method
*/
	{
	LOG_FUNC
	LOG1(_L8("Queue Dumping this 0x%08x"), this);
	TSglQueIter<CTxHctlH5Frame> iter(iFrameQue);
	CTxHctlH5Frame *frame = iter++;

	_LIT(KSpace, " ");
	_LIT(KWriteMarker, "W");
	_LIT(KHighMarker, "H");
	_LIT(KReadMarker, "R");

	TInt index = 0;
	while ( index < iWindowSize )	
		{
	LOG6(_L8("%S %S %S this: 0x%08x, frame : 0x%08x, %d"),
			(index == ToWindowIndex(iWritePosition)) ? &KWriteMarker : &KSpace,
			(index == ToWindowIndex(iHighReadPosition)) ? &KHighMarker : &KSpace,
			(index == ToWindowIndex(iReadPosition)) ? &KReadMarker : &KSpace,
			this,
			frame,
			frame->IsValid()
			);
		frame = iter++;
		index++;
		}
	}
#endif

void CHCTLH5Window::Reset()
	{
	LOG_FUNC

	for(TInt i=0;i<iWindowSize;i++)
		{
		CTxHctlH5Frame* frame = iFrameQue.First();
		frame->Reset();
		iFrameQue.Remove(*frame);
		iFrameQue.AddLast(*frame);

#ifdef _DEBUG_WINDOW
		LOG1(_L8("frame reset 0x%08x"),frame);
#endif
		}

	iStartOfWindow = 0;
	iReadPosition  = 0;
	iHighReadPosition = 0;
	iWritePosition = 0;
	iEndOfWindow = iWindowSize - 1;

#ifdef _DEBUG_WINDOW
	LOG(_L8("Reset"));

	TraceWindowVars();
	TraceQueue();
#endif
	}
	
/* *********************************************************************************************
 * Frame Queue - Manages the frame queues for both Reliable and Unreliable data transfer
 * ********************************************************************************************* */
CHCTLH5FrameQueue* CHCTLH5FrameQueue::NewL(CHCTLH5 &aH5)
	{
	LOG_STATIC_FUNC

	CHCTLH5FrameQueue *self = new (ELeave) CHCTLH5FrameQueue(aH5);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CHCTLH5FrameQueue::ConstructL()
	{
	LOG_FUNC

	// Create the queues
	iReliableQueue   = CHCTLH5Window::NewL(KH5ReliableWindowSize, 
		                                     KMaxReliablePayloadSize,
											 KMaxH5WindowSize);

	iUnreliableQueue = CHCTLH5Window::NewL(KH5UnreliableQueueSize,
		                                     KMaxUnreliablePayloadSize,
											 KH5UnreliableQueueSize+1);

	// Room for Slip encoded packet is twice the size of a full BCSP Packet + 2 for the 0xC0 bytes
	iSlipEncodedFrame = HBufC8::NewL(2 + ((KMaxReliablePayloadSize + KH5HeaderBytes + KH5CrcBytes) * 2));
	}

CHCTLH5FrameQueue::CHCTLH5FrameQueue(CHCTLH5& aH5)
  : iH5(aH5)
	{
	LOG_FUNC
	}

CHCTLH5FrameQueue::~CHCTLH5FrameQueue()

	{
	LOG_FUNC

	delete iUnreliableQueue;
	delete iReliableQueue;
	delete iSlipEncodedFrame;
	}


void CHCTLH5FrameQueue::SetSequencer(CHCTLH5Sequencer& aSequencer)
/**
	Simple method to initialise iSequencer with @param &aSequencer
*/
	{
	LOG_FUNC

	iSequencer = &aSequencer;
	}

TInt CHCTLH5FrameQueue::GetNextFrame(TDesC8* &aFrame, TBool& aIsReliable)
/**
	Method to get the next frame from the frame queue and then to build and slip encode it

	@param &aFrame
	@param aIsReliable a reference to a TBool which will be set to ETrue if the
	       Frame is from the reliable queue.
	@return err
*/
	{
	LOG_FUNC

	TInt err = KErrNone;

	CTxHctlH5Frame *frame = iUnreliableQueue->ReadFrame();

	if ( frame )
		{
		// Got unreliable frame
		iUnreliableQueue->Acknowledged(iUnreliableQueue->StartOfWindow()+1); // Remove the first frame from the window
		frame->SetSequence(0);
		aIsReliable = EFalse;
		}
	else
		{
		// Couldn't get an unreliable frame, so ... get a reliable frame
		frame = iReliableQueue->ReadFrame();

		if ( !frame )
			{
			// We can't get a reliable frame either
			err = KErrH5NothingToSend;
			aIsReliable = EFalse;
			}
		else
			{
			// We've got a reliable frame - increment the retry count
			frame->IncrementRetries();
			frame->SetFlags(iSequencer->TxAck(), iH5.CrcEnabled(), ETrue);		// OURS: the DIC only if negotiated
			if ( frame->Retries() >= KTxRetryLimit )
				{
				err = KErrH5MaxRetries;
				}

			aIsReliable = ETrue;
			}
		}

	if ( !err )
		{
		// No errors so far, so SlipEncode the frame into iSlipEncodedFrame
		frame->BuildFrame();
		frame->SlipEncodeFrame(iSlipEncodedFrame->Des());
		aFrame = iSlipEncodedFrame;

#ifdef _DEBUG_WINDOW
		LOG(_L8("Got Frame"));
		LOGHEXRAW(frame->Payload().Ptr(), frame->PayloadLength());

		LOG(_L8("Slip Encoded Frame"));
		LOGHEXRAW(aFrame->Ptr(), aFrame->Length());
#endif
		}
	else
		{
		// Just in case our return code is ignored :->
		aFrame = NULL;
		}

	return err;
	}

TBool CHCTLH5FrameQueue::AckReceived(TInt aAckValue)
/**
	Handle AckReceived @param aAckValue

	call Acknowledged method on relaible queue
	call CanSend()

	@return ETrue if there are unacknowledged packets remaining
	              in the tx window.
*/
	{
	LOG_FUNC

	TBool ret=iReliableQueue->Acknowledged(aAckValue);
	CanSend();
	return ret;
	}

void CHCTLH5FrameQueue::AckTimeout()
/**
	Handle Rx Ack timeout
	
	Inform reliable queue to resend frame
	CanSend
	Call iSequencer->WakeUp() to trigger the sequencer to pull frames for resending from the
	reliable frame queue
*/
	{
	LOG_FUNC

	iReliableQueue->Acknowledged(KErrTimedOut);
	CanSend();
	iSequencer->WakeUp();
	}

TInt CHCTLH5FrameQueue::AddReliableFrame(const TDesC8 &aData, TUint8 aProtocolId)
/**
	Method to add a frame to the Tx Reliable queue
	@param aData - HCI payload
	@param aProtocolId - ProtocolId eg. ACL, Command
*/
	{
	TInt err = KErrNone;
	LOG_FUNC

#ifdef _DEBUG_WINDOW
	LOG(_L8("Adding Frame to Reliable Queue"));
	LOGHEXDESC(aData);
#endif

	CTxHctlH5Frame *frame=iReliableQueue->WriteFrame();
	
	// A NULL frame is now returned if (on the rare occasion) the frame queue has become blocked.
	// The original panic has been replaced by an upward error handling path. 
	if (!frame)
	 	{
	 	err = KErrOverflow;	
	 	}
	else
		{
		frame->SetProtocolId(aProtocolId);
		frame->SetPayload(aData);
		iSequencer->WakeUp();
		}
	
	CanSend();
	return err;
	
	}

void CHCTLH5FrameQueue::AddUnreliableFrame(const TDesC8 &aData, TUint8 aProtocolId, 
											 TUint8 aAck,
											 TBool aCRCEnabled)
/**
	Method to add an Unreliable frame to the unreliable frame queue

	@param aData - HCI payload
	@param aProtocolId
	@param aAck
	@param aCRCState

	Calls wakeup method on the sequencer to trigger frame sending
*/
	{
	LOG_FUNC

	CTxHctlH5Frame *frame=iUnreliableQueue->WriteFrame();

#ifdef _DEBUG_WINDOW
	LOG(_L8("Adding Frame to Unreliable Queue"));
#endif

	LOGHEXDESC(aData);
	if ( frame )
		{
		frame->SetProtocolId(aProtocolId);
		frame->SetPayload(aData);
		frame->SetFlags(aAck, aCRCEnabled, EFalse);
		}
	// else we've got no free slots - therefore since this is an unreliable Frame we can
	// just throw it away

	iSequencer->WakeUp();
	}

void CHCTLH5FrameQueue::AddUnreliableFrame(TUint8 aProtocolId,
											 TUint8 aAck,
											 TBool aCRCEnabled)
/**
	Method to add an empty Unreliable frame to the unreliable frame queue
	eg. an AckFrame

	@param aProtocolId
	@param aAck
	@param aCRCState

*/
	{
	LOG_FUNC

	CTxHctlH5Frame *frame=iUnreliableQueue->WriteFrame();

#ifdef _DEBUG_WINDOW
	LOG(_L8("Adding Empty Frame to Unreliable Queue"));
#endif

	if ( frame )
		{
		frame->SetProtocolId(aProtocolId);
		frame->SetPayload();
		frame->SetFlags(aAck, aCRCEnabled, EFalse);
		}
	// else we've got no free slots - therefore since this is an unreliable Frame we can
	// just throw it away

	iSequencer->WakeUp();

	}
	
void CHCTLH5FrameQueue::Reset()
 	{
	LOG_FUNC

 	iReliableQueue->Reset();
 	iUnreliableQueue->Reset();
 	}
 	
void CHCTLH5FrameQueue::CanSend()
/**
	Kicks packet router
	Only reliable queue is checked here for availability because we don't want to flow control 
	the unreliable packets. Unreliable packets are simply dropped when the queue is full.
*/
	{
	LOG_FUNC

	iH5.CanSend(iReliableQueue->FreeFrames()>0);
	}
