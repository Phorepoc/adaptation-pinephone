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

#ifndef H5UTILS_H
#define H5UTILS_H

#include <e32def.h>

/**
The panic category for the H5 HCTL.
*/
_LIT(KH5PanicCat, "HCTL H5 Panic");

/**
Panic codes for the H5 HCTL.
*/
enum TH5Panic
	{
	EUnexpectedLinkEvent		    = 0,
	EBadFramePointer				= 1,
	EBadH5Frame					= 2,
	EBadLinkPointer					= 3,
	EPortNotOpen					= 4,
	EReliableQueueFull				= 5,
	EH5IllegalProtocolId			= 6,
	EH5InvalidAckValue			= 7,
	EH5InvalidSequenceValue		= 8,
	EUnexpectedPowerState			= 9,
	EUnexpectedReset				= 10,
	EUnexpectedControllerMgrState	= 11,
	EInvalidCurrentTask				= 12,
	EH5LinkStateOutOfBounds		= 13
	};

#endif // H5UTILS_H

