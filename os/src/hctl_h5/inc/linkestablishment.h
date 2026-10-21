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


#ifndef LINKESTABLISHMENT_H
#define LINKESTABLISHMENT_H

#include <e32base.h>

class CDelay;
class CHCTLH5;

class TLinkState;
class CLinkStateFactory;

NONSHARABLE_CLASS(CLinkEstablishment) :public CBase
/**
CLinkEstablishment
Provides the initial link establishment entity
Owns
	CLinkStateFactory (State Pattern)
  	
This class is not intended for derivation.
*/
	{
public:
	void QueueReadForNextFrame(void);
	static CLinkEstablishment* NewL(CHCTLH5& aHCTLH5);
	~CLinkEstablishment();
	void ProcessLinkMsg(const TDesC8& aLinkMsg);


	void SendLinkMsg(const TDesC8& aLinkMsg);

	void SetState(const TLinkState& aLinkState);   //<Change state of device


	//TLinkState Stub Functions

	void HandleLinkMsgSync();
	void HandleLinkMsgSyncResp();
	void HandleLinkMsgConf();
	void HandleLinkMsgConfResp();

	void StartShyTimer();
	void StartConfTimer();

	void StopShyTimer();
	void StopConfTimer();

	void Choke();
	void UnChoke();
	
 	void HandlePeerReset();
 	void Reset();	

	TBool Muzzled();
	void ResetMuzzled();
private:
	CLinkEstablishment(CHCTLH5& aHCTLH5);

    void ConstructL();	

	void ProcessConfResp();
	void ProcessSyncResp();
	void LinkStateMachine(const TDesC8& aLinkMsg);	//processes received link messages
	void HandleConfTimeout();
	void HandleShyTimeout();
	static TInt ConfTimeout(TAny* aLink);
	static TInt ShyTimeout(TAny* aLink);

private:
	enum TH5LinkStateTransition
		{
		ESyncRecvd,
		EConfRecvd
		};
		
	CHCTLH5& iHctlH5;
	CLinkStateFactory* iLinkStateFactory;
	const TLinkState* iLinkState;

	CDelay* iShyTimer;
	CDelay* iConfTimer;

	TInt iConfCnt;
	};
 #endif //LINKESTABLISHMENT_H
 
