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

#ifndef HCTL_H5_DEBUG_H
#define HCTL_H5_DEBUG_H

#include <bluetooth/logger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, LOG_COMPONENT_HCTL_BCSP);	// OURS: the H5 HCTL logs under the BCSP component id (btlogger has no H5 one)
#endif

#ifdef _DEBUG
#	define __DEBUG_FLOG_RAW_UART_ //enable UART Flogging
#	define __DEBUG_FLOG_STACK_TX_ //enable UART Flogging
#	define __DEBUG_RxDecodedFrame__ //Enable flogging for RxDecodedFrame
#	define __DEBUG_TxDatagramDecodedFrame__ //Enable flogging for RxDecodedFrame
#	define __DEBUG_PacketRouter__ //Enable Flogging for PacketRouter and Processing
#	define __DEBUG_FLOG_RX_ //Enable RX Flogging in CHCTLH5Receiver::ProcessDataL()
#	define __DEBUG_SEQ_VERBOSE__ //Enable flogging for Sequencer
#	define __DEBUG_SEQ_Values__ //Enable flogging of Sequencer Ack Seq etc.
#else
#	undef __DEBUG_FLOG_RAW_UART_ //enable UART Flogging
#	undef __DEBUG_FLOG_STACK_TX_ //enable UART Flogging
#	undef __DEBUG_RxDecodedFrame__ //Enable flogging for RxDecodedFrame
#	undef __DEBUG_TxDatagramDecodedFrame__ //Enable flogging for RxDecodedFrame
#	undef __DEBUG_PacketRouter__ //Enable Flogging for PacketRouter and Processing
#	undef __DEBUG_FLOG_RX_ //Enable RX Flogging in CHCTLH5Receiver::ProcessDataL()
#	undef __DEBUG_SEQ_VERBOSE__ //Enable flogging for Sequencer
#	undef __DEBUG_SEQ_Values__ //Enable flogging of Sequencer Ack Seq etc.
#endif

#endif //HCTL_H5_DEBUG_H
