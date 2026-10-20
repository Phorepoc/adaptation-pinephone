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

#include "hctlh5sender.h"

#include "hctlh5sequencer.h"
#include "debug.h"

#include <d32comm.h>


/**
Implementation of Class CHCTLH5Sender
*/
CHCTLH5Sender::CHCTLH5Sender(CHCTLH5Sequencer& aSequencer, RBusDevComm& aPort) : 
	CActive(EPriorityStandard),
	iSequencer(aSequencer),
	iPort(aPort)
    {
	LOG_FUNC
	CActiveScheduler::Add(this);
    }

CHCTLH5Sender::~CHCTLH5Sender()
    {
	LOG_FUNC

	Cancel();
    }

CHCTLH5Sender* CHCTLH5Sender::NewL(CHCTLH5Sequencer& aSequencer, RBusDevComm& aPort)
    {
	LOG_STATIC_FUNC

    CHCTLH5Sender* self=new(ELeave)CHCTLH5Sender(aSequencer, aPort);
    return self;
    }

TInt CHCTLH5Sender::Write(const TDesC8& aData)
    {
	LOG_FUNC
#ifdef __DEBUG_FLOG_RAW_UART_	
	LOG(_L8("HCTLH5: UART TX..."));
	LOGHEXDESC(aData);
#endif

	iPort.Write(iStatus, aData);
 	SetActive();

    return KErrNone;
    }

void CHCTLH5Sender::RunL()
    {
	LOG_LINE
	LOG_FUNC
	LOG1(_L8("\tiStatus = %d"), iStatus.Int());

	iSequencer.SendNextPacket();
    }

void CHCTLH5Sender::DoCancel()
	{
	LOG_FUNC
	
	iPort.WriteCancel(); 
	}
