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


#ifndef HCTLH5SEQUENCER_H
#define HCTLH5SEQUENCER_H

#include <e32base.h>

class CDelay;
class CHCTLH5;
class RBusDevComm;
class CHCTLH5FrameQueue;
class CHCTLH5Sender;
class CRxHctlH5Frame;

NONSHARABLE_CLASS(CHCTLH5Sequencer) :public CBase
/**
CHCTLH5Sequencer
Provides the Sequencing layer as described in the BCSP Spec. this permits reliable retransmission
of ACL and Command Data and in the final version of BCSP a windowing mechanism allowing multiple 
frames < 7 to be transmitted without acknowledgment for each individual frame.
  	
This class is not intended for derivation.
*/
	{
public:
	static CHCTLH5Sequencer* NewL(CHCTLH5& aHCTLH5, RBusDevComm& aPort,CHCTLH5FrameQueue& aH5FrameQueue);
	~CHCTLH5Sequencer();
	
	void WakeUp();			// Set the state machine to ESending and then in Entry() does the same as SendNextPacket()
	void SendNextPacket();	// Gets next packet from iBcspFrameQueue and Sends it directly to the UART
	void HandleRxAck();		// Triggers removal of Acked frames
	void SendPacket(TDesC8* aFrame, TBool aIsReliable);

	TInt SendQueuedFrame();

	void HandleRx(CRxHctlH5Frame* aFrame);
	
	TUint8 TxAck() const;
	void ResendState();
	void HandleRxAckTimeout();

 	//BCSP peer reset functions
 	void Reset();
 	void HandlePeerReset();

private:
	CHCTLH5Sequencer(CHCTLH5& aHCTLH5, CHCTLH5FrameQueue& aH5FrameQueue);
    void ConstructL(RBusDevComm& aPort);
	void UpdateTxAckVal();
	void StartRxAckTimer();
	void StartTxAckTimer();
	void TxAckMsg();
	
	static TInt TxTimeout(TAny* aThis);
	static TInt RxTimeout(TAny* aThis);

private:
	enum TH5RelRxState
		{
		EIdle,
		EReceiving
		};

	TUint8 iRxAck;	//Updated by HandleRx()
	TUint8 iRxSeq;
	TUint8 iRxSeqExpected;
	TUint8 iRxSeqLast;
	TUint8 iPacketAvail;

	TUint8 iTxAck;

	TH5RelRxState iRelRxState;

	TUint8 iRxTimeoutCount;
	CDelay* iRxAckTimer;
	CDelay* iTxAckTimer;

	CHCTLH5& iHctlH5;
	CHCTLH5FrameQueue& iFrameQueue;

	CHCTLH5Sender* iSender;
	};
	
#endif // HCTLH5SEQUENCER_H
