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


#ifndef HCTLH5SENDER_H
#define HCTLH5SENDER_H

#include <e32base.h>

class CHCTLH5Sequencer;
class RBusDevComm;

NONSHARABLE_CLASS(CHCTLH5Sender) : public CActive
/**
CHCTLH5Sender
Implements BCSP specific implementation of UART CHCTLUartSender Active Object
  	
This class is not intended for derivation.
*/
    {
public:
    static CHCTLH5Sender* NewL(CHCTLH5Sequencer& aSequencer, RBusDevComm& aPort);
    ~CHCTLH5Sender();         
    TInt Write(const TDesC8& aData);

private:    
	// from CActive
    virtual void RunL();     
    virtual void DoCancel();
    
    CHCTLH5Sender(CHCTLH5Sequencer& aSequencer, RBusDevComm& aPort);

private: 
	// unowned
	CHCTLH5Sequencer&	iSequencer;
	RBusDevComm& iPort;
    };
#endif //HCTLH5SENDER_H

