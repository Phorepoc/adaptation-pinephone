// hctlh5controllermanager.h -- OURS (Bluetooth rung R2, 2026-10-10): the H5 HCTL's controller manager = the power
// and hard-reset interfaces of the HCI framework over the chip's enable pin. Forked from the 2010 BCSP controller
// manager (Nokia, EPL v1.0; hctlbcspcontrollermanager.h), whose power/reset went through CSR BCCMD cold-reset /
// cold-halt commands on BCSP channel 2 and a "CTS low for 10 s" power observer. Here: power off = DTR low (our
// UART1 PDD maps the port's DTR to BT_EN PL4 + device-wake PH6); power on and hard reset = the Linux power pulse,
// then an H5 link re-establishment, whose completion is the state change we report.
/**
 @file
 @internalComponent
*/

#ifndef HCTLH5CONTROLLERMANAGER_H
#define HCTLH5CONTROLLERMANAGER_H

#include <e32base.h>
#include <bluetooth/hci/hctluartbase.h>
#include <bluetooth/hci/hctlpowerinterface.h>

class MControllerStateObserver;
class CHCTLH5;

NONSHARABLE_CLASS(CHCTLH5ControllerManager) : public CBase, public MHCTLPowerInterface
    {
public:
	enum TControllerManagerState
		{
		EIdle,
		EWaiting,
		EResetHardware,		// the chip was power-cycled; the next link establishment completes the task
		};

	enum TControllerManagerTask
		{
		ENoTask,
		EPowerDown,
		EPowerUp,
		EControllerReset
		};

    static CHCTLH5ControllerManager* NewL(CHCTLH5& aHCTLH5);
    ~CHCTLH5ControllerManager();

	void SetObserver(MControllerStateObserver& aObserver);
	void Start();
	void HardReset();
	TBool H5LinkEstablished();
	TBool ExpectedControllerReset();
	TBool PowerOffRequested();

private:
	TInt MhpiGetPower(TBTPowerState& aState);
	TInt MhpiSetPower(const TBTPowerState aState);

    CHCTLH5ControllerManager(CHCTLH5& aHCTLH5);
	TInt DoSetPower(TBTPowerState aState);

	static TInt HandlePowerDown(TAny* aThis);
	void DoHandlePowerDown();

	static TInt HandleReadyToResetController(TAny* aThis);
	void DoHandleReadyToResetController();

	TBool DoH5LinkEstablishedForPowerUp();
	TBool DoH5LinkEstablishedForControllerReset();

private:
	// Unowned
    MControllerStateObserver* iObserver;
	CHCTLH5& iHCTLH5;

	// Owned
	TControllerManagerState iControllerManagerState;
	TBTPowerState iLastPowerRequest;
	TBTPowerState iCurrentPowerState;
	TControllerManagerTask iCurrentTask;
	CAsyncCallBack* iPowerDownCallback;
	CAsyncCallBack* iReadyToResetControllerCallback;
    };

#endif // HCTLH5CONTROLLERMANAGER_H
