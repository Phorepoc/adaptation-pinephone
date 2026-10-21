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

#ifndef HCTLH5FRAMEQUEUE_H
#define HCTLH5FRAMEQUEUE_H

#include <e32base.h>

// To Turn on window debuggng #define _DEBUG_WINDOW
#undef _DEBUG_WINDOW

class CHCTLH5Sequencer;
class CHCTLH5;
class CTxHctlH5Frame;

NONSHARABLE_CLASS(CHCTLH5Window) : public CBase
/**
Window and associated functions to contain a queue of frames
handles acknowledgement of a particular received frame and moves the window onwards
within the scope of total number of sequence IDs

Is only used by CHCTLH5FrameQueue
*/
	{
public:
	static CHCTLH5Window *NewL(TInt aWindowSize, TInt aFrameSize, TInt aNumberOfSeqIds);
	~CHCTLH5Window();

	TInt FreeFrames();

    CTxHctlH5Frame* WriteFrame(); // Gets the write frame and moves the WritePosn
	CTxHctlH5Frame* ReadFrame();  // Gets the read frame and moves the ReadPosn

	TBool Acknowledged(TInt AckNo);
	TInt StartOfWindow() const;
 	void Reset();

private:
	void ConstructL(TInt aFrameSize);
	CHCTLH5Window(TInt aWindowSize, TInt aNumberOfSeqIds);

	// Two simple inline private methods to reduce complexity of code whilst maintaining
	// efficiency
	TInt ToSeqId(TInt aIndex) const;
	TInt ToWindowIndex(TInt aSeqId) const; 
	void IncrementSeqId(TInt &aSeqId) const;
	CTxHctlH5Frame *GetFrame(TInt aIndex);

	// Internal consistency checking for the class
    void __DbgTestInvariant() const;
	
#ifdef _DEBUG_WINDOW
	void TraceQueue();
	void TraceWindowVars() const;
#endif


private:
	TInt iStartOfWindow;
	TInt iReadPosition;
	TInt iHighReadPosition;
	TInt iWritePosition;
	TInt iEndOfWindow;
	TInt iWindowSize;
	TInt iNumberOfSeqIds;
	TSglQue<CTxHctlH5Frame> iFrameQue;

	// This class stores most of its indices as SequneceIds, the only exception being
	// iWindowSize which is used in the conversion between SequenceIds and WindowIndex.
	//
	//   SequenceIds          Window Index
	//                     |
	//   iStartOfWindow    +   .
	//                     |   .
	//   iReadIndex        |   .
	//                     |   .
	//   iHighReadIndex    |   .
	//                     |   .
	//   iWriteIndex       |   .
	//                     |   .
	//   iEndOfWindow      +   iWindowSize
	//                     |
	//                     |
	//                     |
	//   iNumberOfSeqIds   |
	};

NONSHARABLE_CLASS(CHCTLH5FrameQueue) : public CBase
/**
This is used by CHCTLH5 to queue frames for transmission
the frames queued are stored in CHCTLH5Window
there are two windows used 
	one for reliable frames
	one for unreliable frames
*/
	{
public:
	static CHCTLH5FrameQueue* NewL(CHCTLH5 &aH5);
	~CHCTLH5FrameQueue();

	void SetSequencer(CHCTLH5Sequencer& aSequencer);

	// Methods called by sequencer
	// Gets the next frame for sending 
	// (aFrame is the slip encoded frame
	//  returned by this method)
	//
	// returns: -
	//
	// KErrNone          - all is fine
	// KErrNothingToSend - nothing 
	//                     available to
	//                     send
	// KErrMaxRetries    - exceeded max 
	//                     retries for a
	//                     frame
	TInt GetNextFrame(TDesC8* &aFrame, TBool& aIsReliable);

	TBool AckReceived(TInt aAckValue); // Ack has been Received
	void AckTimeout();                 // Rx Ack Timer has expired

	// Methods called by CHCTLH5
	TInt AddReliableFrame  (const TDesC8 &aData, TUint8 aProtocolId);
	void AddUnreliableFrame(const TDesC8 &aData, TUint8 aProtocolId, 
												 TUint8 aAck,
												 TBool aCRCEnabled);

	void AddUnreliableFrame(TUint8 aProtocolId, 
							TUint8 aAck,
							TBool aCRCEnabled);
	void Reset();

private:
	void CanSend(); 
	void ConstructL();
	CHCTLH5FrameQueue(CHCTLH5& aH5);

private:
	// Not owned by this class
	CHCTLH5Sequencer* iSequencer; // For calls to Wakeup()
	CHCTLH5& iH5; // For calls to CanSend()

	// Owned by this class
	CHCTLH5Window* iReliableQueue;
	CHCTLH5Window* iUnreliableQueue;

	HBufC8* iSlipEncodedFrame;
	};

#endif // HCTLH5FRAMEQUEUE_H
