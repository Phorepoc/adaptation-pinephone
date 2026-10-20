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

