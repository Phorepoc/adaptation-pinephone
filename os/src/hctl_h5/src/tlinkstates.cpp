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

#include "hctlh5linkstate.h"
#include "hctlh5consts.h"
#include "linkestablishment.h"
#include "h5utils.h"
#include "debug.h"

TLinkState::TLinkState(CLinkStateFactory& aFactory, CLinkEstablishment& aLink) 
	: iFactory(aFactory), 
	  iLink(aLink)
	{
	LOG_FUNC
	}

TLinkState::~TLinkState()
	{
	LOG_FUNC
	}


void TLinkState::Entry() const
	{
	LOG_FUNC

	PANIC(KH5PanicCat, EUnexpectedLinkEvent);
	}

void TLinkState::Exit() const
	{
	LOG_FUNC

	PANIC(KH5PanicCat, EUnexpectedLinkEvent);
	}

void TLinkState::HandleLinkMsgConf() const
	{
	LOG_FUNC
	}

void TLinkState::HandleLinkMsgConfResp() const
	{
	LOG_FUNC
	}

void TLinkState::HandleLinkMsgSync() const
	{
	LOG_FUNC
	}

void TLinkState::HandleLinkMsgSyncResp() const
	{
	LOG_FUNC
	}


TLinkStateShy::TLinkStateShy (CLinkStateFactory& aFactory, CLinkEstablishment& aLink) 
	: TLinkState(aFactory, aLink)
	{
	LOG_FUNC
	}

TLinkStateShy::~TLinkStateShy()
	{
	LOG_FUNC
	}


void TLinkStateShy::Entry() const
	{
	LOG_FUNC

	iLink.Choke();
	if(iLink.Muzzled() == EFalse)
		{
		iLink.StartShyTimer();
		}
	iLink.QueueReadForNextFrame();
	}

void TLinkStateShy::Exit() const
	{
	LOG_FUNC

	iLink.StopShyTimer();
	}

void TLinkStateShy::HandleLinkMsgSync() const
	{
	LOG_FUNC

	iLink.SendLinkMsg(KH5LinkMsg_SyncResp);
	}

void TLinkStateShy::HandleLinkMsgSyncResp() const
	{
	LOG_FUNC

	iLink.SetState(iFactory.State(CLinkStateFactory::ECurious));
	}

void TLinkStateShy::HandleLinkMsgConf() const
	{
	LOG_FUNC

	// Ignore message
	}



TLinkStateCurious::TLinkStateCurious (CLinkStateFactory& aFactory, CLinkEstablishment& aLink)
	: TLinkState(aFactory, aLink)
	{
	LOG_FUNC
	}

TLinkStateCurious::~TLinkStateCurious()
	{
	LOG_FUNC
	}


void TLinkStateCurious::Entry() const
	{
	LOG_FUNC

	iLink.StartConfTimer();
	}

void TLinkStateCurious::Exit() const
	{
	LOG_FUNC

	iLink.StopConfTimer();
	}

void TLinkStateCurious::HandleLinkMsgConf() const
	{
	LOG_FUNC

	iLink.SendLinkMsg(KH5LinkMsg_ConfResp);
	}

void TLinkStateCurious::HandleLinkMsgConfResp() const
	{
	LOG_FUNC

	iLink.SetState(iFactory.State(CLinkStateFactory::EGarrulous));
	}

void TLinkStateCurious::HandleLinkMsgSync() const
	{
	LOG_FUNC

	iLink.SendLinkMsg(KH5LinkMsg_SyncResp);
	}


void TLinkStateCurious::HandleLinkMsgSyncResp() const
	{
	LOG_FUNC
	}



TLinkStateGarrulous::TLinkStateGarrulous (CLinkStateFactory& aFactory, CLinkEstablishment& aLink) 
	: TLinkState(aFactory, aLink)
	{
	LOG_FUNC
	}

TLinkStateGarrulous::~TLinkStateGarrulous()
	{
	LOG_FUNC
	}


void TLinkStateGarrulous::Entry() const
	{
	LOG_FUNC

	iLink.UnChoke();	//Connected
	}

void TLinkStateGarrulous::Exit() const
	{
	LOG_FUNC
	}

void TLinkStateGarrulous::HandleLinkMsgConf() const
	{
	LOG_FUNC

	iLink.SendLinkMsg(KH5LinkMsg_ConfResp);
	}

void TLinkStateGarrulous::HandleLinkMsgConfResp() const
	{
	LOG_FUNC
	}

void TLinkStateGarrulous::HandleLinkMsgSync() const
	{
	LOG_FUNC

	//Peer Reset Detection SetState EShy
	LOG(_L("HCI CONNECTION RESET BY PEER"));
	//Purge all buffers Datagram Queue and Sequence Queue
	//Reset Sequence layer
	//Signal Stack that HCI link is down 
 	iLink.HandlePeerReset();	
	}
