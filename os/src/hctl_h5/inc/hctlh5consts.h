/* Portions Copyright (c) Nokia Corporation and/or its subsidiary(-ies)
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// hctlh5consts.h -- OURS (Bluetooth rung R2, 2026-10-10): the constants of the H5 (Three-wire UART, Bluetooth Core
// Vol 4 Part D) HCTL. Forked from the 2010 BCSP HCTL's hctlbcspconsts.h (Nokia, EPL v1.0): the header bits, the
// checksum, the CRC and the SLIP framing are identical between BCSP and H5; what changes is the packet-type
// numbering (BCSP channels -> H5 packet types) and the link-establishment messages (4-byte magic words -> the
// 2-byte SYNC/CONFIG messages with a configuration field).
/**
 @file
 @internalComponent
*/

#ifndef HCTLH5CONSTS_H
#define HCTLH5CONSTS_H

/** H5 packet types (Vol 4 Part D table 4.1); the BCSP channel numbers these replace are in the comments */
const TUint8 KH5AckChnl=0x00;				// pure acknowledgement (BCSP channel 0)
const TUint8 KH5CommandChnl=0x01;			// HCI command, host -> controller, reliable (BCSP 5 carried both directions)
const TUint8 KH5ACLDataChnl=0x02;			// ACL data, reliable (BCSP channel 6)
const TUint8 KH5SynchronousDataChnl=0x03;	// SCO data, unreliable (BCSP channel 7)
const TUint8 KH5EventChnl=0x04;				// HCI event, controller -> host, reliable
const TUint8 KH5IsoChnl=0x05;				// ISO data (Core 5.2; never in this stack)
const TUint8 KH5VendorChnl=0x0e;			// vendor specific
const TUint8 KH5LinkChnl=0x0f;				// link control (BCSP channel 1)

//FlagByte Constants
const TUint8 KH5CrcPresentMask = 0x40;
const TUint8 KH5ProtocolTypeMask = 0x80;

const TUint8 KH5SeqMask = 0x07;
const TUint8 KH5AckFieldOffset = 3;
const TUint8 KH5AckMask = 0x38;

const TUint8 KH5PayloadLen1stBitsMask=0xf0;
const TUint8 KH5ProtocolIdMask=0x0f;

/**HCTL Header and trailer consts*/
const TUint8 KH5HeaderFlagsByteIndex = 0;
const TUint8 KH5HeaderByte2Index = 1;
const TUint8 KH5HeaderByte3Index = 2;
const TUint8 KH5HeaderChecksumByteIndex = 3;

const TUint8 KH5HeaderBytes = 4;
const TUint8 KH5CrcBytes = 2;

/**Defined max payload sizes of various packet types*/
const TUint16 KMaxSCOData = 64;
const TUint16 KMaxHCICommand = 256;				// the Realtek 0xfc20 download fragment is exactly 3 + 253 bytes
const TUint16 KMaxACLData = 1021;

const TUint16 KMaxReliablePayloadSize=KMaxACLData+5;
const TUint16 KMaxUnreliablePayloadSize=KMaxSCOData+4;

/**Defined max windows size i.e. no. packets that can be transmitted without acknowledgement */
const TUint8  KH5ReliableWindowSize = 4;		// what we ask for in CONFIG; the controller's CONFIG_RSP may shrink it
const TUint8  KH5UnreliableQueueSize = 4;
const TUint8  KH5WindowSize=4;
const TUint8  KMaxH5WindowSize=8;				// 3-bit sequence numbers

const TUint16 KHCIHeaderSize=4;

/** Flags field masks */
const TInt KH5UnreliableDatagramQueue=0;
const TInt KH5ReliableSequenceQueue=1;

/**Sequence layer constants */
const TUint8 KWindowSize=4;				// Define according to efficiency and performance
const TUint8 KMaxWindowSize=8;
const TInt KRxAckTimeout=250000;			// Linux hci_h5: a 250 ms ack / retransmit timer
const TInt KTxAckTimeout=125000;
const TUint8 KTxRetryLimit=20;

/**Link Establishment layer constants (Vol 4 Part D 8): 2-byte messages, CONFIG and CONFIG_RSP may carry one
configuration field byte -- sliding window b0-2, OOF flow control b3, data integrity check (CRC) b4, version b5-7 */
_LIT8(KH5LinkMsg_Sync,     "\x01\x7e");
_LIT8(KH5LinkMsg_SyncResp, "\x02\x7d");
_LIT8(KH5LinkMsg_Conf,     "\x03\xfc");		// + our configuration field (KH5OurConfigField)
_LIT8(KH5LinkMsg_ConfResp, "\x04\x7b");		// the controller's carries the agreed field; ours, in reply, none
_LIT8(KH5LinkMsg_Wakeup,   "\x05\xfa");
_LIT8(KH5LinkMsg_Woken,    "\x06\xf9");
_LIT8(KH5LinkMsg_Sleep,    "\x07\x78");
const TUint8 KH5OurConfigField = (KH5ReliableWindowSize & 0x07) | 0x10;	// window 4 + CRC, as Linux and Realtek's tool
const TUint8 KH5ConfigWindowMask = 0x07;
const TUint8 KH5ConfigCrcMask = 0x10;
const TInt KH5ConfCntMax=50;
const TInt KTShyTimeout = 250000;		// SYNC every 250 ms until SYNC_RSP (Linux: 100 ms, Realtek's tool: 250 ms)
const TInt KTConfTimeout = 250000;		// CONFIG every 250 ms until CONFIG_RSP

/** The controller's power sequence (Linux hci_h5 rtl: enable + device-wake low for 100 ms, then high for 500 ms).
Our UART1 PDD maps the port's DTR signal to the enable (PL4) + device-wake (PH6) pins (src/phorepoc-arm/a64/bt). */
const TInt KH5PowerOffHoldTime = 100000;
const TInt KH5PowerOnSettleTime = 500000;

/**SLIP Encoding Bytes */
const TUint8 KSlipWrapperByte = 0xc0;
const TUint8 KSlipByteDB = 0xdb;
const TUint8 KSlipByteDC = 0xdc;
const TUint8 KSlipByteDD = 0xdd;

/**H5 Framing errors*/
const TInt KErrSlipCorrupted=-6100;
const TInt KErrH5HeaderCorrupt=-6101;
const TInt KErrH5CorruptedHCIPayload=-6102;
const TInt KErrH5CRCCheckFailed=-6103;
const TInt KErrH5UnRecognizableHCIData=-6104;
const TInt KErrH5PacketTypeUnrecognized=-6105;
const TInt KErrH5InvalidAckFlagValue=-6106;
const TInt KErrH5InvalidSeqFlagValue=-6107;
const TInt KErrH5WriteACLDataFailed=-6108;
const TInt KErrH5WriteCommandDataFailed=-6109;
const TInt KErrH5NothingToSend = -6110;
const TInt KErrH5MaxRetries = -6111;
const TInt KErrH5WriteBcCmdDataFailed=-6112;

const TUint KHCTLRecvBufSize=4100;		// one SLIP frame of the largest ACL packet, escapes included (BCSP: 2048)

#endif // HCTLH5CONSTS_H
