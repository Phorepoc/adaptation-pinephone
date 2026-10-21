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

