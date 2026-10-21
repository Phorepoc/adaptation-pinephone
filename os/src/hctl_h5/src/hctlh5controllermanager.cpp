// Portions Copyright (c) Nokia Corporation and/or its subsidiary(-ies)
// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// hctlh5controllermanager.cpp -- OURS (Bluetooth rung R2, 2026-10-10): see hctlh5controllermanager.h. Forked from
// the 2010 BCSP controller manager (Nokia, EPL v1.0); the state/task bookkeeping and the asynchronous completion
// callbacks are BCSP's, the BCCMD commands are replaced by the enable-pin pulse.
/**
 @file
 @internalComponent
*/

#include "hctlh5controllermanager.h"
#include "debug.h"
#include "hctlh5.h"
#include "h5utils.h"
#include <e32debug.h>
#include <bluetooth/hci/controllerstateobserver.h>

CHCTLH5ControllerManager* CHCTLH5ControllerManager::NewL(CHCTLH5& aHCTLH5)
    {
	LOG_STATIC_FUNC
    CHCTLH5ControllerManager* self = new(ELeave) CHCTLH5ControllerManager(aHCTLH5);
	CleanupStack::PushL(self);
	TCallBack cbHandlePowerDown(HandlePowerDown, self);
	self->iPowerDownCallback = new(ELeave) CAsyncCallBack(cbHandlePowerDown, CActive::EPriorityStandard);
	TCallBack cbHandleReadyToResetController(HandleReadyToResetController, self);
	self->iReadyToResetControllerCallback = new(ELeave) CAsyncCallBack(cbHandleReadyToResetController, CActive::EPriorityStandard);
	CleanupStack::Pop(self);
    return self;
    }

CHCTLH5ControllerManager::~CHCTLH5ControllerManager()
    {
	LOG_FUNC
 	delete iPowerDownCallback;
	delete iReadyToResetControllerCallback;
	}

CHCTLH5ControllerManager::CHCTLH5ControllerManager(CHCTLH5& aHCTLH5)
 :	iHCTLH5(aHCTLH5),
 	iControllerManagerState(EIdle),
 	iLastPowerRequest(EBTOn),		// the HCTL powers the chip when it opens the port (CHCTLH5::PortOpenedL)
 	iCurrentPowerState(EBTOn),
	iCurrentTask(ENoTask)
	{
	LOG_FUNC
    }

void CHCTLH5ControllerManager::Start()
/*
For future use - called when HCTL is started
*/
	{
	LOG_FUNC
	}

TInt CHCTLH5ControllerManager::MhpiGetPower(TBTPowerState& aState)
	{
	if(iCurrentTask != ENoTask)
		//Information will not be reliable
		{
		return KErrInUse;
		}
	aState = iCurrentPowerState;
	return KErrNone;
	}

TInt CHCTLH5ControllerManager::MhpiSetPower(TBTPowerState aState)
/*
To be called from outside the class only, for example in response to user request
*/
	{
	RDebug::Printf("[hctl_h5] SetPower(%s) task %d state %d", aState == EBTOn ? "on" : "off", iCurrentTask, iCurrentPowerState);
	TInt rerr = KErrNone;
	switch(iCurrentTask)
		{
		case EPowerUp:
		case EPowerDown:
			//If SetPower is called whilst performing a power request error the BT client.
			//There should only be one!
			rerr = KErrInUse;
			break;
		case EControllerReset:
			//Just postpone power request. A power down will be handled when
			//the next link establshment comes in.
			iLastPowerRequest = aState;
			rerr = KErrInUse;
			break;
		case ENoTask:
			iLastPowerRequest = aState;
			if(iCurrentPowerState == iLastPowerRequest)
				{
				//Nothing to do - need to return an error to complete synchronously
				//(Do not tell stack.)
				rerr = KErrAlreadyExists;
				}
			else
				{
				rerr = DoSetPower(aState);
				}
			break;
		default:
			PANIC(KH5PanicCat, EInvalidCurrentTask);
		};
	return rerr;
	}

TInt CHCTLH5ControllerManager::DoSetPower(TBTPowerState aState)
	{
	switch(aState)
		{
		case EBTOff:
			{
			// no SYNC while the chip is off; the link goes back to its initial state; the pin goes low
			iHCTLH5.SetMuzzled();
			iHCTLH5.Reset();
			iHCTLH5.PowerOff();
			iPowerDownCallback->CallBack();	//allows asynch callback to BTClient.
			iCurrentTask = EPowerDown;
			iControllerManagerState = EWaiting;
			}
			break;
		case EBTOn:
			{
			// the power pulse, then a fresh link establishment; H5LinkEstablished() completes the task
			iHCTLH5.ResetMuzzled();
			iHCTLH5.PowerPulse();
			iCurrentTask = EPowerUp;
			iControllerManagerState = EResetHardware;
			iHCTLH5.Reset();
			}
			break;
		default:
			PANIC(KH5PanicCat, EUnexpectedPowerState);
			break;
		};
	return KErrNone;
	}

void CHCTLH5ControllerManager::HardReset()
	{
	// Only perform reset if the power is on and the controller is
	// not performing another action.
	if(iLastPowerRequest == EBTOn && iCurrentTask == ENoTask)
		{
		iReadyToResetControllerCallback->CallBack();//call back will reset the controller
		iCurrentTask = EControllerReset;
		iControllerManagerState = EWaiting;
		}
	}

/*
	Called when the H5 link reaches ACTIVE.
	Controller Manager may not want H5 to inform the outside world,
	for example, if it wishes to do a controller reset, and
	so wishes to manage the controller itself and keep the outside world at bay.
*/
TBool CHCTLH5ControllerManager::H5LinkEstablished()
	{
	if(iLastPowerRequest == EBTOff)
		/*
		Should only get here if a power off request occurred whilst
		awaiting this link establishment - in which case abandon
		whatever we were doing and switch power off.
		*/
		{
		DoSetPower(EBTOff);
		return EFalse;
		}

	TBool doTellStack = ETrue;
	switch(iCurrentTask)
		{
		case ENoTask:
			break; //not to do with us
		case EPowerUp:
			{
			doTellStack = DoH5LinkEstablishedForPowerUp();
			}
			break;
		case EControllerReset:
			{
			doTellStack = DoH5LinkEstablishedForControllerReset();
			}
			break;
		case EPowerDown:
		default:
			{
	    	__ASSERT_DEBUG(EFalse, PANIC(KH5PanicCat, EInvalidCurrentTask));
			}
		}
	return doTellStack;
	}

/**
	Called when the controller starts trying to establish a link whilst we
	think a link is established. This state is intentional if the controller manager
	has just power-cycled the chip.
*/
TBool CHCTLH5ControllerManager::ExpectedControllerReset()
	{
	return (iControllerManagerState == EResetHardware);
	}

TBool CHCTLH5ControllerManager::PowerOffRequested()
	{
	return (iLastPowerRequest == EBTOff);
	}

TBool CHCTLH5ControllerManager::DoH5LinkEstablishedForPowerUp()
	{
	__ASSERT_DEBUG(iControllerManagerState == EResetHardware, PANIC(KH5PanicCat, EUnexpectedControllerMgrState));
	if(iControllerManagerState == EResetHardware)
		{
		if(iObserver)
			{
			iObserver->McsoProcessPowerChange(KErrNone, MControllerStateObserver::EBTFatalChange, EBTOn);
			}
		iCurrentPowerState = EBTOn;
		iCurrentTask = ENoTask;
		iControllerManagerState = EIdle; //controller reset finished
		}
	return ETrue;
	}

TBool CHCTLH5ControllerManager::DoH5LinkEstablishedForControllerReset()
	{
	TBool doTellStack = ETrue;
	switch(iControllerManagerState)
		{
		case EResetHardware:
			{
			if(iObserver)
				{
				iObserver->McsoProcessHardResetPhaseChange(KErrNone, MControllerStateObserver::EBTFatalChange, EBTResetComplete);
				}
			iCurrentTask = ENoTask;
			iControllerManagerState = EIdle; //controller reset finished
			}
			break;
		case EIdle:
			break;
		default:
			{
			PANIC(KH5PanicCat, EUnexpectedControllerMgrState);
			}
		}
	return doTellStack;
	}

/*static*/TInt CHCTLH5ControllerManager::HandlePowerDown(TAny* aThis)
	{
	LOG_STATIC_FUNC
	reinterpret_cast<CHCTLH5ControllerManager*>(aThis)->DoHandlePowerDown();
	return KErrNone;
	}

void CHCTLH5ControllerManager::DoHandlePowerDown()
	{
	LOG_FUNC
	if(iObserver)
		{
		iObserver->McsoProcessPowerChange(KErrNone, MControllerStateObserver::EBTFatalChange, EBTOff);
		}
	iCurrentPowerState = EBTOff;
	iCurrentTask = ENoTask;
	iControllerManagerState = EIdle;
	}

/*static*/TInt CHCTLH5ControllerManager::HandleReadyToResetController(TAny* aThis)
	{
	LOG_STATIC_FUNC
	reinterpret_cast<CHCTLH5ControllerManager*>(aThis)->DoHandleReadyToResetController();
	return KErrNone;
	}

void CHCTLH5ControllerManager::DoHandleReadyToResetController()
	{
	LOG_FUNC
	// asynchronous call to tell stack hard reset has started
	RDebug::Printf("[hctl_h5] hard reset started");
	if(iObserver)
		{
		iObserver->McsoProcessHardResetPhaseChange(KErrNone, MControllerStateObserver::EBTFatalChange, EBTResetStarted);
		}
	iHCTLH5.PowerPulse();
	iControllerManagerState = EResetHardware;
	iHCTLH5.Reset(); //causes H5 to re-establish the link (causing a call back to CHCTLH5::UnChoke())
	}

/**
 Setter for the observer
 @param aObserver A event observer the power man can use to notify power changes
 */
void CHCTLH5ControllerManager::SetObserver(MControllerStateObserver& aObserver)
	{
	iObserver = &aObserver;
	}
