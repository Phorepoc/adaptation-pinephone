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

#ifndef HCTLH5LINKSTATE_H
#define HCTLH5LINKSTATE_H

#include <e32base.h>

class TLinkState;
class CLinkEstablishment;


NONSHARABLE_CLASS(CLinkStateFactory) : public CBase
/**
Provides factory object for Link Establishment State Pattern

Holds states (see BCSP Spec.)
	EShy
	ECurious
	EGarrulous
*/
	{
public:
	friend class CLinkEstablishment;
	enum TH5LinkState
		{
		EShy,
		ECurious,
		EGarrulous,
		MaxStates
		};

	const TLinkState& State(const TH5LinkState aState) const;
	~CLinkStateFactory ();

private:
	static CLinkStateFactory* NewL(CLinkEstablishment& aLink);
	CLinkStateFactory();

	// Array of all the states
	TFixedArray<TLinkState*, MaxStates> iStates;
	};

NONSHARABLE_CLASS(TLinkState)
/**
Provides base class for BCSP Link States
*/
	{
public:
	TLinkState (CLinkStateFactory& aFactory, CLinkEstablishment& aLink); 
	virtual ~TLinkState();
	
	virtual void Entry() const;
	virtual void Exit() const;

	virtual void HandleLinkMsgSync() const;
	virtual void HandleLinkMsgSyncResp() const;
	virtual void HandleLinkMsgConf() const;
	virtual void HandleLinkMsgConfResp() const;

protected:
	CLinkStateFactory& iFactory;  // Get states
	CLinkEstablishment& iLink;
	};
	

NONSHARABLE_CLASS(TLinkStateShy) : public TLinkState
/**
TLinkStateShy
EShy state handling object
  	
This class is not intended for derivation.
*/
	{
public:
	TLinkStateShy (CLinkStateFactory& aFactory, CLinkEstablishment& aLink); 
	~TLinkStateShy ();

	void Entry() const;
	void Exit() const;

	void HandleLinkMsgSync() const;
	void HandleLinkMsgSyncResp() const;
	void HandleLinkMsgConf() const;
	};

NONSHARABLE_CLASS(TLinkStateCurious) : public TLinkState
/**
TLinkStateCurious
ECurious state handling object
  	
This class is not intended for derivation.
*/
	{
public:
	TLinkStateCurious (CLinkStateFactory& aFactory, CLinkEstablishment& aLink); 
	~TLinkStateCurious ();
	
	void Entry() const;
	void Exit() const;

	void HandleLinkMsgSync() const;
	void HandleLinkMsgConf() const;
	void HandleLinkMsgConfResp() const;
	void HandleLinkMsgSyncResp() const;
	};

NONSHARABLE_CLASS(TLinkStateGarrulous) : public TLinkState
/**
TLinkStateGarrulous
EGarrulous state handling object
  	
This class is not intended for derivation.
*/
	{
public:
	TLinkStateGarrulous (CLinkStateFactory& aFactory, CLinkEstablishment& aLink); 
	~TLinkStateGarrulous ();

	void Entry() const;
	void Exit() const;

	void HandleLinkMsgSync() const;
	void HandleLinkMsgConf() const;
	void HandleLinkMsgConfResp() const;
	};

#endif // HCTLH5LINKSTATE_H
