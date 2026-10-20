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

#include "hctlh5frame.h"

#include "h5utils.h"
#include "hctlh5consts.h"
#include "debug.h"

const TUint16 KCrcTable[] = 
/**
	Global CRC table
*/
	{
	0x0000, 0x1081, 0x2102, 0x3183,
	0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xa50a, 0xb58b,
	0xc60c, 0xd68d,	0xe70e, 0xf78f
	};

/**
Implementation of Class THctlH5PacketType
*/


/**
	Method to set BCSP ProtocolId
*/
TInt THCTLH5PacketType::SetProtocolId(TUint8 aProtocolId)
	{
	LOG_FUNC

#ifdef __ARMCC__
// Suppressing the warning "Pointless comparison of unsigned integer with zero"
// as although a constant may be zero it is present so as to make the code more
// understandable.
# pragma diag_suppress 186
#endif // __ARMCC__

	// H5 packet types: 0 ack, 1 command, 2 ACL, 3 SCO, 4 event, 5 ISO, 14 vendor, 15 link control (6-13 reserved)
if ((aProtocolId > KH5IsoChnl && aProtocolId < KH5VendorChnl))
	{
	LOG1(_L8("THCTLH5PacketType: Header unsupported, this used to be a BCSP 6 panic, now error handled. aProtocolId = %d"), aProtocolId);
	return KErrNotSupported;
	}	               
	               
#ifdef __ARMCC__
# pragma diag_default 186
#endif // __ARMCC__

	iProtocolId = aProtocolId;
	return KErrNone;
	}

TUint8 THCTLH5PacketType::ProtocolId() const
	{
	return iProtocolId;
	}

/**
Implementation of Class THctlH5Flags
*/

TBool THCTLH5Flags::CrcEnabled() const
	{
	return (iFlagsField & KH5CrcPresentMask);
	}

TUint8 THCTLH5Flags::Ack() const
	{
	return ((iFlagsField & KH5AckMask) >> KH5AckFieldOffset);
	}

TUint8 THCTLH5Flags::Sequence() const
	{
	return (iFlagsField & KH5SeqMask);
	}

TBool THCTLH5Flags::IsReliableProtcolType() const
	{
	return (iFlagsField & KH5ProtocolTypeMask);
	}

TUint8 THCTLH5Flags::FlagsField() const
	{
	return iFlagsField;
	}

/**
	Trivial setter will panic if we attempt to set an invalid value
*/
void THCTLH5Flags::SetAck(TUint8 aValue) 
	{
	__ASSERT_DEBUG((aValue < KMaxWindowSize), PANIC(KH5PanicCat, EH5InvalidAckValue));
	// Clear the existing value and set the new value.
	iFlagsField &= ~KH5AckMask;
	iFlagsField |= (aValue << KH5AckFieldOffset);
	}

/**
	Trivial setter will panic if we attempt to set an invalid value
*/
void THCTLH5Flags::SetSequence(TUint8 aSeq)
	{
	__ASSERT_DEBUG((aSeq < KMaxWindowSize), PANIC(KH5PanicCat, EH5InvalidSequenceValue));
	iFlagsField &= ~KH5SeqMask;
	iFlagsField |= aSeq;
	}

/**
	Trivial setter
*/
void THCTLH5Flags::SetCrcEnabled(TBool aCrcEnabled)
	{
	if(aCrcEnabled)
		{
		iFlagsField |= KH5CrcPresentMask;
		}
	else
		{
		iFlagsField &= ~KH5CrcPresentMask;
		}
	}

void THCTLH5Flags::SetIsReliableProtcolType(TBool aIsReliableProtcolType)
	{
	if(aIsReliableProtcolType)
		{
		iFlagsField |= KH5ProtocolTypeMask;
		}
	else
		{
		iFlagsField &= ~KH5ProtocolTypeMask;
		}
	}

void THCTLH5Flags::SetFlagsField(TUint8 aFlagsField)
	{
	iFlagsField = aFlagsField;
	}
	
/**
Implementation of Class CHCTLH5Frame
*/
CHCTLH5Frame::CHCTLH5Frame()
	{
	}

/**
	Method to set the flags in the Header byte of a BCSP frame
*/
void CHCTLH5Frame::SetFlags(TUint8 aAck, TBool aIsCrcEnabled, TBool aIsReliableProtcolType)
	{
	LOG_FUNC
	iFlagHeaderByte.SetAck(aAck);
	iFlagHeaderByte.SetCrcEnabled(aIsCrcEnabled);
	iFlagHeaderByte.SetIsReliableProtcolType(aIsReliableProtcolType);
	}

void CHCTLH5Frame::SetSequence(TUint8 aSeq)
	{
	iFlagHeaderByte.SetSequence(aSeq);
	}

TInt CHCTLH5Frame::SetProtocolId(TUint8 aProtocolId)
	{
	return iProtocolId.SetProtocolId(aProtocolId);
	}

TBool CHCTLH5Frame::CrcEnabled() const
	{
	return iFlagHeaderByte.CrcEnabled();
	}

TUint8 CHCTLH5Frame::CheckSum() const
	{
	return iCheckSum;
	}

TUint8 CHCTLH5Frame::ProtocolId() const
	{
	return iProtocolId.ProtocolId();
	}

TUint8 CHCTLH5Frame::FlagsField() const
	{
	return iFlagHeaderByte.FlagsField();
	}

TUint16 CHCTLH5Frame::PayloadLength() const
	{
	return static_cast<TUint16>(iPayloadPtr.Size());
	}

TInt CHCTLH5Frame::ValidateChecksum(const TDesC8& aHeader)
/**
	Method to validate the checksum of a received frame header
*/
	{
	LOG_FUNC
	TInt rerr = KErrNone;

	if (aHeader.Length() < KH5HeaderBytes)
		{
		rerr = KErrH5HeaderCorrupt;
		}
	else if(aHeader[KH5HeaderChecksumByteIndex] != CalcCheckSum(aHeader))
		{
		rerr = KErrH5HeaderCorrupt;
		}

	return rerr;
	}

TDesC8& CHCTLH5Frame::Payload()
/**
	Trivial getter method
*/
	{
	return iPayloadPtr;
	}

TUint8 CHCTLH5Frame::Sequence() const
/**
	Trivial getter method
*/
	{
	return iFlagHeaderByte.Sequence();
	}

TUint8 CHCTLH5Frame::Ack() const
/**
	Trivial getter method
*/
	{
	return iFlagHeaderByte.Ack();
	}

TBool CHCTLH5Frame::IsReliableProtcolType() const
/**
	Trivial getter method
*/
	{
	return iFlagHeaderByte.IsReliableProtcolType();
	}

TUint16 CHCTLH5Frame::CalcCRC(const TDesC8& aData)
/**
 	@param aData - Parameters Buffer upon which to calculate crc
 	@return CRC Value TUint16
*/
 	{
 	LOG_FUNC
  	TUint16 crcValue = 0xffff;	// Fill the shift register with 1s
  	TUint8 dataByte;
  	
  	for (TInt i=0;i<aData.Length();i++)
  		{
  		dataByte = aData[i];
  		crcValue = static_cast<TUint16>((crcValue >> 4) ^ KCrcTable[(crcValue^dataByte) & 0xf]);
  		crcValue = static_cast<TUint16>((crcValue >> 4) ^ KCrcTable[(crcValue^(dataByte >> 4)) & 0xf]);
  		}
  	return ReverseCrcBits(crcValue); // Reverse all the bits in the CRC
  	}

TUint16 CHCTLH5Frame::ReverseCrcBits(TUint16 aCRCValue)
/**
	Method to reverse the CRC Value
	@param aCRCValue
*/
	{
	TUint16 result = 0; 
	for(TInt b=0;b<16;b++)
		{
		result = static_cast<TUint16>(result << 1) ;
		result |= (aCRCValue & 1);
		aCRCValue = static_cast<TUint16>(aCRCValue >> 1);
		}
	return result;
	}



TUint8 CHCTLH5Frame::CalcCheckSum(const TDesC8& aHeader)
/**
	Method to calculate the checksum
	The Checksum error is used to check the integrity of the header:
*/
	{
	LOG_FUNC
	__ASSERT_DEBUG(aHeader.Length() >= KH5HeaderBytes, PANIC(KH5PanicCat, EBadH5Frame));
	TUint8 tempsum = static_cast <TUint8>(aHeader[KH5HeaderFlagsByteIndex] + 
	                                      aHeader[KH5HeaderByte2Index] + 
	                                      aHeader[KH5HeaderByte3Index]);
	return ~tempsum;
	}

void CHCTLH5Frame::Reset()
/**
	Method to reset the frame
*/
	{
	LOG_FUNC
	iIsValid = EFalse;
	}

TBool CHCTLH5Frame::IsValid() const
/**
	Trvial getter method
*/
 	{
 	return iIsValid;
 	}


/**
Implementation of Class CTxHctlH5Frame
*/

CTxHctlH5Frame* CTxHctlH5Frame::NewLC(const TDesC8& aHCIPayload)
	{
	LOG_STATIC_FUNC
	CTxHctlH5Frame* self= new(ELeave) CTxHctlH5Frame();
	CleanupStack::PushL(self);
	self->ConstructL(aHCIPayload);
	return self;
	}

CTxHctlH5Frame* CTxHctlH5Frame::NewLC(TInt aFrameSize)
	{
	LOG_STATIC_FUNC
	CTxHctlH5Frame* self= new(ELeave) CTxHctlH5Frame();
	CleanupStack::PushL(self);
	self->ConstructL(aFrameSize);
	return self;
	}

CTxHctlH5Frame* CTxHctlH5Frame::NewL(const TDesC8& aHCIPayload)
	{
	LOG_STATIC_FUNC
	CTxHctlH5Frame *self=NewLC(aHCIPayload);
	CleanupStack::Pop(self);
	return self;
	}

CTxHctlH5Frame* CTxHctlH5Frame::NewL(TInt aFrameSize)
	{
	LOG_STATIC_FUNC
	CTxHctlH5Frame *self=NewLC(aFrameSize);
	CleanupStack::Pop(self);
	return self;
	}

void CTxHctlH5Frame::ConstructL(const TDesC8& aHCIPayload)
	{
	LOG_FUNC

	// Instantiates iFrame and the TPtr8 iFramePtr to it
	// Then calls the SetPayload() method with the param aHCIPayload
	iFrame = HBufC8::NewL(aHCIPayload.Length() + KH5HeaderBytes + KH5CrcBytes);
	iFramePtr.Set(iFrame->Des());

	SetPayload(aHCIPayload);
	}

void CTxHctlH5Frame::ConstructL(TInt aFrameSize)
	{
	LOG_FUNC

	// Instantiates iFrame and the TPtr8 iFramePtr to it
	// then calls the SetPayload() method.
	iFrame = HBufC8::NewL(KH5HeaderBytes + KH5CrcBytes + aFrameSize);
	iFramePtr.Set(iFrame->Des());

	SetPayload();
	}

void CTxHctlH5Frame::SetPayload(const TDesC8 &aPayload)
	{
	iFramePtr.SetMax();
	TPtr8 payload(iFramePtr.MidTPtr(KH5HeaderBytes));

	payload = aPayload.Right(payload.MaxSize());
	iPayloadPtr.Set(payload);

	iFramePtr.SetLength(KH5HeaderBytes + aPayload.Size());

	iIsValid = ETrue;
	}

void CTxHctlH5Frame::SetPayload()
	{
	LOG_FUNC
	iPayloadPtr.Set(KNullDesC8);

	iFramePtr.SetLength(KH5HeaderBytes);

	iIsValid = ETrue;
	}

CTxHctlH5Frame::CTxHctlH5Frame()
  : iFramePtr(NULL, 0, 0)
	{
	LOG_FUNC
	}

CTxHctlH5Frame::~CTxHctlH5Frame()
	{
	LOG_FUNC
	delete iFrame;
	}

void CTxHctlH5Frame::BuildFrame()
/**
	Build  BCSP frame method

	Zero the frame Ptr
	Set the frame length to KMaxHeaderBytes - to address the BCSP header bytes

	Populate all 4 header bytes with the Flag byte,PayloadLength, ProtocolId and checksum

	Set the frame length to Payloadlength + KMaxBcspHeaderBytes

	Populate the BCSP frame payload

	Check if the CRC flag is set if it is then Calculate the CRC of the Whole Frame and append the crc

*/
	{
	LOG_FUNC
	iFramePtr.SetLength(KH5HeaderBytes);
	iFramePtr[KH5HeaderFlagsByteIndex] = iFlagHeaderByte.FlagsField();
	iFramePtr[KH5HeaderByte2Index] = static_cast<TUint8>(ProtocolId() | ((PayloadLength() & 0xf) << 4));
	iFramePtr[KH5HeaderByte3Index] = static_cast<TUint8>((PayloadLength() & 0xff0) >> 4);
	iFramePtr[KH5HeaderChecksumByteIndex] = CalcCheckSum(iFramePtr);

	iFramePtr.SetLength(PayloadLength() + KH5HeaderBytes);

	if (PayloadLength() > 0)
		{
		iPayloadPtr.Set(iFramePtr.Ptr() + KH5HeaderBytes, PayloadLength());
		}

	if (CrcEnabled())
		{
		TUint16 crcValue = CalcCRC(iFramePtr.Left(PayloadLength() + KH5HeaderBytes));
		iFramePtr.Append((crcValue & 0xff00) >> 8); //Append first byte of CRC
		iFramePtr.Append(crcValue & 0x00ff);        //Append second Byte of CRC
		}
	}

void CTxHctlH5Frame::Reset()
/**
	Reset the BCSP frame and set retries to 0
*/
	{
	LOG_FUNC
	CHCTLH5Frame::Reset();
	iRetries = 0;
	}

void CTxHctlH5Frame::SlipEncodeFrame(TPtr8 aSlipFrame)
/**
	SlipEncoding method
	SLIP is outlined in RFC 1055. The principle is to surround each packet
	with a start and an end special byte. This allows the beginning and the end of a packet to be
	found and consequently the length of the packet to be known.
	The packet encoding and decoding scheme has two basic rules:
	Because the special byte can only appear in the stream as a delimiter, it has to be replaced inside
	the payload by an escape sequence, composed of two bytes: 0xc0 is mapped to 0xDB 0xDC.
	Consequently 0xDB is also a special byte, and each occurrence of this escape byte is replaced
	by an another sequence of two bytes: 0xDB 0xDD
	This method is very quick and simple for encoding a packet. The corresponding Decoding
	procedure is used for received packets.
	Moreover, as explained in the section regarding detecting errors, the SLIP layer provides a
	method of error detection.
*/
	{
	LOG_FUNC
	__ASSERT_DEBUG(iFrame != NULL, PANIC(KH5PanicCat, EBadFramePointer));

	aSlipFrame.Zero();
	aSlipFrame.SetMax();

	TInt i=0;
	TInt j=0;

	aSlipFrame[j++] = KSlipWrapperByte;

	for (i=0;i<iFrame->Length();i++)
		{
		switch((*iFrame)[i])
			{
			case KSlipWrapperByte:
				{
				aSlipFrame[j++] = KSlipByteDB;
				aSlipFrame[j++] = KSlipByteDC;
				break;
				}
			case 0xdb:
				{
				aSlipFrame[j++] = KSlipByteDB;
				aSlipFrame[j++] = KSlipByteDD;
				break;
				}

			default:
				{
				aSlipFrame[j++] = (*iFrame)[i];
				break;
				}
			}
		}

	aSlipFrame[j++] = KSlipWrapperByte;
	aSlipFrame.SetLength(j);
	}



/**
Implementation of Class CRxHctlH5Frame
*/


CRxHctlH5Frame* CRxHctlH5Frame::NewL()
/**
	CRxHctlH5Frame::NewL() method
	
	@return self
*/
	{
	LOG_STATIC_FUNC
	return new(ELeave) CRxHctlH5Frame;
	}

TInt CRxHctlH5Frame::SlipDecodeFrame(const TDesC8& aReceivedFrame)
/**
	Slip decoding methopd for received frames
*/
	{
	LOG_FUNC
	TInt rerr = KErrNone;

 	// TPtrC8 packet = aReceivedFrame;
	TPtr8 packet(const_cast<TUint8*>(aReceivedFrame.Ptr()),
			     aReceivedFrame.Size(),
				 aReceivedFrame.Size());

	if ((packet.Length() < KH5HeaderBytes) || (packet[packet.Length()-1] != KSlipWrapperByte) || (packet[0] == KSlipWrapperByte))
		{
		/** 
		Fail slip decoding for one of or none of these reasons:
			1. first byte is another slip wrapper byte (One has been received already and discarded in terminating reads)
			   should not receive 2 consecutive slip wrapper byte in any one slip packet
			2. The final packet byte is not a slip wrapper byte
		
		NB. A good slip Packet at this stage should have x-bytes appended with one slip wrapper byte.
		This is because the first slip wrapper byte was already picked up on the first terminated read.
		*/
		rerr = KErrSlipCorrupted;
		}
	else
		{
		packet.SetLength(packet.Length() - 1); // Remove the last 0xc0 byte.

		TInt  i = 0;
		TInt  offset = 0;
		TInt  length = packet.Length();

		while ( (i<length) && (rerr == KErrNone) )
			{
			switch (packet[i])
				{
				case 0xc0:	// If this appears do nothing as it should be either the header or trailer byte checked above
					rerr = KErrSlipCorrupted;
					break;

				case 0xdb:
					i++; // Check the next byte immediately
					offset--;
					if ( i<length )
						{
						switch (packet[i])
							{
							case 0xdc:
								packet[i+offset] = 0xc0;
								break;

							case 0xdd:
								packet[i+offset] = 0xdb;
								break;

							default:
								rerr = KErrSlipCorrupted;
							};
						}
					else
						{
						rerr = KErrSlipCorrupted;
						}
					break;

				default:
					if ( offset )
						{
						packet[i+offset] = packet[i];
						}
					break;
				}
			i++;
			}

		if(rerr == KErrNone)
			{
			packet.SetLength(length + offset);
			rerr = Set(packet);
			}
		}

	return rerr;
	}

TInt CRxHctlH5Frame::Set(const TDesC8& aFrame)
/**
	Populate a BCSP frame object and vaildate its contents via the various BCSP error 
	checking methods
*/
	{
	LOG_FUNC
	TInt err = ValidateChecksum(aFrame);	// Do Checksum check
	TInt errId = KErrNone;
	TUint16 payloadLen = 0;

	if (err == KErrNone)
		{
		ParseFlagByte(aFrame);	// populate THctlH5Flags members
		
		errId = ParsePayloadLenProtId(aFrame, payloadLen);	
		if (errId)
			{
			return errId;	// corrupted Id, no point in continuing 	
			}

		// Check header payload length field is the same as the actual HCIPayloadLength
		if (CrcEnabled())
			{
			TInt actualPayloadLen = aFrame.Length() - KH5CrcBytes - KH5HeaderBytes;
			if(actualPayloadLen >= 0) // Check payload is sufficient length
				{
				iPayloadPtr.Set(aFrame.Ptr() + KH5HeaderBytes, actualPayloadLen);

				if (PayloadLength() != payloadLen) // Check payload length
					{
					LOG(_L8("CRxHctlH5Frame: KErrH5CorruptedHCIPayload..."));
					err = KErrH5CorruptedHCIPayload;
					}
				else if (CheckCRC(aFrame) != KErrNone) // Do CRC Check
					{
					LOG(_L8("CRxHctlH5Frame: KErrBCSPCRCCheckFailed..."));
					err = KErrH5CRCCheckFailed;
					}
				}
			else
				{
				LOG(_L8("CRxHctlH5Frame: KErrH5CorruptedHCIPayload..."));
				err = KErrH5CorruptedHCIPayload;
				}
			}
		else 
			{
			TInt actualPayloadLen = aFrame.Length() - KH5HeaderBytes;
			// There is no need to check that the payload is of sufficient length since
			// the ValidateChecksum call would have failed and stopped us getting here.
			__ASSERT_DEBUG(actualPayloadLen >= 0, PANIC(KH5PanicCat, EBadH5Frame));
			iPayloadPtr.Set(aFrame.Ptr() + KH5HeaderBytes, actualPayloadLen);
			if (PayloadLength() != payloadLen) 
				{
				LOG(_L8("HCTLH5: KErrH5CorruptedHCIPayload..."));
				LOGHEXDESC(aFrame);
				err = KErrH5CorruptedHCIPayload;
				}
			}
		}

	iIsValid = (err == KErrNone) ? ETrue : EFalse;

	return err;
	}


CRxHctlH5Frame::CRxHctlH5Frame()
  : CHCTLH5Frame()
	{
	}

void CRxHctlH5Frame::ParseFlagByte(const TDesC8& aHeader)
	{
	LOG_FUNC
	LOG1(_L8("Parse HeaderByte %02x"), aHeader[KH5HeaderFlagsByteIndex]);

	iFlagHeaderByte.SetFlagsField(aHeader[KH5HeaderFlagsByteIndex]);
	}

TInt CRxHctlH5Frame::ParsePayloadLenProtId(const TDesC8& aHeader, TUint16& aRetPayloadLen)
/**
	Method to extract the Payload Length and Protocol Id from the 2nd and 3rd header bytes
*/
	{
	LOG_FUNC
	TInt err = KErrNone;

	err = SetProtocolId((aHeader[KH5HeaderByte2Index] & KH5ProtocolIdMask));
	if (!err)
		{
		aRetPayloadLen = static_cast<TUint16>(((aHeader[KH5HeaderByte2Index] & KH5PayloadLen1stBitsMask) >> 4) | (aHeader[KH5HeaderByte3Index] << 4));
		}
	
	return err;
	}

TInt CRxHctlH5Frame::CheckCRC(const TDesC8 &aFrame)
/**
	Method to check and validate the CRC
*/
	{
	LOG_FUNC
	TPtrC8 frame(aFrame.Ptr(), aFrame.Size() - KH5CrcBytes);

	TUint16 tempCRCvalue;
	TUint16 crcValue = CalcCRC(frame);	// Calculates and sets iCRCValue

	//compare with hctl trailer CRC value
	TBuf8<2> crcval;
	crcval = aFrame.Right(KH5CrcBytes);
	
	tempCRCvalue = static_cast<TUint16>((crcval[0] << 8) + crcval[1]);

	return ((crcValue == tempCRCvalue) ? KErrNone : KErrH5CRCCheckFailed);
	}
