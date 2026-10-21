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

#ifndef HCTLH5RECEIVER_H
#define HCTLH5RECEIVER_H

#include <e32base.h>

class RBusDevComm;
class CHCTLH5;

NONSHARABLE_CLASS(CHCTLH5Receiver) : CActive
/**
CHCTLH5Receiver
Implements BCSP specific implementation of UART CHCTLUartReceiver Active Object

Any errors at this stage will cause the RX frame to be discarded and another read to be queued on the UART

If there are no errors then control will be passed to the sequence layer CHCTLH5Sequencer via CHCTLH5::HandleRx()
Then another read will be queued on the UART
  
This class is not intended for derivation.
*/
    {
public:
    static CHCTLH5Receiver* NewL(CHCTLH5& aHCTLH5, RBusDevComm& aPort);
    ~CHCTLH5Receiver();
 	virtual void ProcessData();	
	void QueueReadForNextFrame();

private:
    virtual void RunL();
	virtual TInt RunError(TInt aError);
    CHCTLH5Receiver(CHCTLH5& aHCTLH5, RBusDevComm& aPort);
	void ConstructL();
	void DoCancel();
private:
	// Set the receive buffer equal to the 3-DH5 packet size.
	// This value should be changed by licensees if the controller
	// is exceeding this packet size.
	
	// 3-DH5.  Payload = 1021 octets.
	// Buffer length = payload + HCTL overhead
	// OURS: a full 1021-byte ACL packet SLIP-encoded can exceed 2 KB; the LDD's own receive buffer is grown
	// to match in CHCTLH5::DoConfigL (hctlh5consts.h KHCTLRecvBufSize is the same figure)
	static const TUint16 KHCTLRecvBufSize = 4100;

	CHCTLH5& iHctlH5;
    RBuf8 iReceiveBuffer;
    TPtr8 iReceiveBufPtr;
	RBusDevComm& iPort;
    };

#endif // HCTLH5RECEIVER_H


