// Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the Eclipse Public License v2.0, available at
// https://www.eclipse.org/legal/epl-2.0/ (relicensed from v1.0 under
// section 7 of the Eclipse Public License v1.0).
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License, v. 2.0 are satisfied: GNU General Public License, version 2
// or later.
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors
//
// Description:
//

/**
 @file
 @internalComponent
*/

#include "hctlh5receiver.h"

#include "hctlh5.h"
#include "hctlh5consts.h"
#include "debug.h"


/**
Implementation of Class CHCTLH5Receiver
*/
CHCTLH5Receiver::CHCTLH5Receiver(CHCTLH5& aHCTLH5, RBusDevComm& aPort) :
	CActive(EPriorityStandard), 
	iHctlH5(aHCTLH5),
	iReceiveBufPtr(NULL,0),
	iPort(aPort)
    {
	LOG_FUNC
	CActiveScheduler::Add(this);
    }

CHCTLH5Receiver::~CHCTLH5Receiver()
    {
	LOG_FUNC

	Cancel();
    iReceiveBuffer.Close();
    }

CHCTLH5Receiver* CHCTLH5Receiver::NewL(CHCTLH5& aHCTLH5, RBusDevComm& aPort) 
    {
	LOG_STATIC_FUNC

    CHCTLH5Receiver* self=new(ELeave)CHCTLH5Receiver(aHCTLH5, aPort);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

void CHCTLH5Receiver::ProcessData()	
/**
	This method parses the byte stream for a pair of 0xC0 bytes denoting beginning and end of packets
	Then calls the HandleRx
	Then zeros the pointer to the receive buffer and queues another read for the next frame
*/
    {
	LOG_FUNC

#ifdef __DEBUG_FLOG_RAW_UART_	
	LOG(_L8("HCTLH5: UART RX..."));
	TPtrC8 ptr = iReceiveBufPtr.Ptr();
	TInt len = iReceiveBufPtr.Length();
	LOGHEXDESC(iReceiveBufPtr);
#endif
	
	if(iReceiveBufPtr.Length() > 1) // Processing the remainder of the SLIP encoded Frame
		{
		if(iHctlH5.HandleRx(iReceiveBufPtr) != KErrNone)	// SLIP Decode Frame
			{
			LOG(_L8("HCTLH5: SLIP Decoding failed discard packet requeue read..."))
			}
		}

	// Reset Buffers
	iReceiveBufPtr.Zero();
	QueueReadForNextFrame(); // Get more stuff until next slip byte
	}

void CHCTLH5Receiver::QueueReadForNextFrame()
/**
	Method moved due to HCI refactoring to reduce code duplication
	This method should suffice for all RBusDevComm reads

	@param aBytesRequired
*/
	{
	LOG_FUNC

	if (IsActive())
        {
		LOG(_L8("Receiver already active - TRYING TO CANCEL AND REQUEUE"));
 		Cancel(); // consume signal then do re-queue
        }
 
    SetActive();

	iPort.Read(iStatus, iReceiveBufPtr);
	}

void CHCTLH5Receiver::RunL()
/**
	If no error occurred, call ProcessData(). Otherwise, throw any data away 
	and requeue the Read. 	
*/
	{
	LOG_LINE
	LOG_FUNC
	LOG1(_L8("\tiStatus = %d"), iStatus.Int());

	switch(iStatus.Int())
		{
		case KErrNone:
			// Receiver has a framer state machine
			ProcessData();
			break;
		
		case KErrAbort:
		case KErrBadPower:
			iReceiveBufPtr.Zero();
			// Pretend nothing happened and requeue the read.
			// Indeed the driver gives us this notification, not on power-down
			// but on the next power-up, so we can requeue.
			QueueReadForNextFrame();
			break;

#ifdef __DEBUG_INIT_PROGRESS
		case KErrCommsOverrun:
		case KErrCommsFrame:
		case KErrCommsParity:
			break;

		default:
			PANIC(KHCTLH5PanicCat, EHCIHCTLReceiverFramerBadState);
			break;
#else
		default:
			// Ignore error condition.
			iReceiveBufPtr.Zero();
			QueueReadForNextFrame();
			break;
#endif
		};
	}

void CHCTLH5Receiver::ConstructL()
    {
	LOG_FUNC
	
	//Create a HBufC based RBuf Object. 
    HBufC8* buf = HBufC8::NewMaxL(KHCTLRecvBufSize);
    iReceiveBuffer.Assign(buf);
    iReceiveBufPtr.Set(buf->Des());
    }

void CHCTLH5Receiver::DoCancel()
	{
	LOG_FUNC
	
	iPort.ReadCancel();
	}

TInt CHCTLH5Receiver::RunError(TInt aError)
	{
	LOG_LINE
	LOG_FUNC
	LOG1(_L8("\taError = %d"), aError);
	static_cast<void>(aError); // prevent unused variable warning
	return KErrNone;
	}
