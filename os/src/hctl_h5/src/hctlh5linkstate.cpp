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

#include "hctlh5linkstate.h"
#include "debug.h"
#include "h5utils.h"


/**
Implementation of State Pattern classes For Link Establishment 
*/

CLinkStateFactory::CLinkStateFactory()
/**
	Default constructor for CLinkStateFactory
	CLinkStateFactory is the factory object in the state pattern for link states:
		TLinkStateShy
		TLinkStateCurious
		TLinkStateGarrulous
*/
	{
	LOG_FUNC
	}

CLinkStateFactory* CLinkStateFactory::NewL(CLinkEstablishment& aLink)
/**
	CLinkStateFactory::NewL() method

	Instatiates itself and its 3 TLinkStates

	@return factory
*/
	{
	LOG_STATIC_FUNC

	CLinkStateFactory* factory= new (ELeave) CLinkStateFactory();
	CleanupStack::PushL(factory);
	// Create the new link states
	factory->iStates[EShy]   = new (ELeave) TLinkStateShy(*factory, aLink);
	factory->iStates[ECurious]  = new (ELeave) TLinkStateCurious(*factory, aLink);
	factory->iStates[EGarrulous]  = new (ELeave) TLinkStateGarrulous(*factory, aLink);
	
	CleanupStack::Pop();
	return factory;
	}

CLinkStateFactory::~CLinkStateFactory()
/**
	Default destructor for CLinkStateFactory
*/
	{
	LOG_FUNC

	// Destroy all the state objects
	iStates.DeleteAll();
	}

const TLinkState& CLinkStateFactory::State(const TH5LinkState aState) const
/**
	Simple method to return a reference to a specific state defined by @param TH5LinkState
	@param aState A enum specifying the desired link state
	@return TLinkState The link state object requested
*/
	{
	LOG_FUNC
	__ASSERT_DEBUG(aState != MaxStates, PANIC(KH5PanicCat, EH5LinkStateOutOfBounds));
	return *iStates[aState];
	}

