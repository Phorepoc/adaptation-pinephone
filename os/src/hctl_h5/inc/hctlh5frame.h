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

#ifndef HCTLH5FRAME_H
#define HCTLH5FRAME_H

#include <e32base.h>

class HCTLH5FrameHelper;

NONSHARABLE_CLASS(THCTLH5PacketType)		
/**
T Class for setting the correct protocol ID for the BCSP frame Header Byte
depends on the packet type
*/
    {
public:
	TInt SetProtocolId(TUint8 aProtocolId);
	TUint8 ProtocolId() const;
private:
	TUint8 iProtocolId;
    };


NONSHARABLE_CLASS(THCTLH5Flags)
/**
T Class for setting the remainder of the information conveyed in the flag header byte
*/
	{
public:
	void SetSequence(TUint8 aSeq); 
	void SetAck(TUint8 aAck); 
	void SetCrcEnabled(TBool aIsCrcEnabled);
	void SetIsReliableProtcolType(TBool aIsReliableProtcolType);
	void SetFlagsField(TUint8 aFlagsField);
	
	TUint8 Sequence() const;
	TUint8 Ack() const;
	TBool CrcEnabled() const;
	TBool IsReliableProtcolType() const;
	TUint8 FlagsField() const;

private:
	TUint8 iFlagsField;
	};

NONSHARABLE_CLASS(CHCTLH5Frame) : public CBase
/**
Class used to populate a BCSP frame with the correct header,trailer data and payload
*/
	{
public:
	void SetFlags(TUint8 aAck, TBool aIsCrcEnabled, TBool aIsReliableProtcolType);
	void SetSequence(TUint8 aSeq);

	TInt SetProtocolId(TUint8 aProtocolId);
	TBool CrcEnabled() const;
	TUint8 ProtocolId() const;
	TUint8 FlagsField() const;
	TBool IsReliableProtcolType() const;
	TUint8 Sequence() const;
	TUint8 Ack() const;
	TUint8 CheckSum() const;
	TUint16 CalcCRC(const TDesC8& aData);

	TUint16 PayloadLength() const;
	TDesC8& Payload();

	TInt ValidateChecksum(const TDesC8& aHeader);
	virtual TBool IsValid() const;
	virtual void Reset();

protected:
	CHCTLH5Frame();
	TUint8 CalcCheckSum(const TDesC8& aHeader);

private:    
	TUint16 ReverseCrcBits(TUint16 aCRCValue);

protected:
	TPtrC8 iPayloadPtr;
	TUint16 iCRCValue;
	TUint8 iCheckSum;
	THCTLH5Flags iFlagHeaderByte;
	TBool iIsValid;

private:
	THCTLH5PacketType	iProtocolId;
	};


NONSHARABLE_CLASS(CTxHctlH5Frame) : public CHCTLH5Frame
/**
CTxHctlH5Frame
Provides an object which embodies a BCSP frame including methods for populating the 
	HCI payload
	Header bytes
	CRC bytes
Provides all error checking (Packet Integrity) excluding SLIP encoding and decoding
*/
	{
	friend class CHCTLH5Window;

public:
	static CTxHctlH5Frame* NewLC(const TDesC8& aHCIPayload);
	static CTxHctlH5Frame* NewLC(TInt aFrameSize=0);
	static CTxHctlH5Frame* NewL(const TDesC8& aHCIPayload);
	static CTxHctlH5Frame* NewL(TInt aFrameSize=0);

	virtual ~CTxHctlH5Frame();

	void SetPayload(const TDesC8 &aPayload);
	void SetPayload();
	
	void BuildFrame();
	void SlipEncodeFrame(TPtr8 aSlipFrame);

	void IncrementRetries() { iRetries++; };
	TInt Retries(void) const { return iRetries; };

	void Reset();

private:
    CTxHctlH5Frame();

    void ConstructL(const TDesC8& aHCIPayload);
	void ConstructL(TInt aFrameSize);

	const TDesC8& BuildHeaderBytes(const TUint8 aFlagHeaderByte, const TUint8 aProtocolId,const TUint16 aPayloadLength);

	TUint16 UpdateCRC(TUint8 aDataByte,TUint16 aCRCValue);
	TUint16 GetBigEndianCRC(TUint16 aCRCValue);

private:
	// When config stuff complete chuck all THctlH5Frame into iSlipEncoded HBufC8* 
	// queue on SequenceBuf or DatagramBuf
	HBufC8* iFrame;	  
	TPtr8 iFramePtr;
	TInt iRetries; // Number of times that this has been sent so far
	TSglQueLink iLink; // For SqlQue and the Window
	};

NONSHARABLE_CLASS(CRxHctlH5Frame) : public CHCTLH5Frame
/**
TRxHctlH5Frame
Provides an object which embodies a BCSP frame including methods for populating the 
	HCI payload
	Header bytes
	CRC bytes
Provides all error checking (Packet Integrity) excluding SLIP encoding and decoding
*/
	{
public:
	static CRxHctlH5Frame *NewL();

	TInt SlipDecodeFrame(const TDesC8& aReceivedFrame);

private:
    CRxHctlH5Frame();
	void ParseFlagByte(const TDesC8& aFlagbyte);
	TInt ParsePayloadLenProtId(const TDesC8& aHeader, TUint16& aRetPayloadLen);
	TInt CheckCRC(const TDesC8& aFrame);
	TInt Set(const TDesC8& aHCIPayload);
	};

#endif // HCTLH5FRAME_H
