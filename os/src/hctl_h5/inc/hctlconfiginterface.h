/* Portions Copyright (c) Nokia Corporation and/or its subsidiary(-ies)
 * Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// hctlconfiginterface.h -- the HCTL configuration interface the TI HCTL defines (2010 release,
// bthci/hci2implementations/hctls/ti/inc/HCTLConfigInterface.h, Nokia, EPL v1.0; @internalTechnology, so not in the
// exported include tree). Our H5 HCTL implements it and our Realtek initialisor calls it after the vendor
// "set baud rate" command completes, exactly as the TI pair does.
/**
 @file
 @internalTechnology
*/

#ifndef HCTLCONFIGINTERFACE_H
#define HCTLCONFIGINTERFACE_H

#include <e32def.h>

/**
The UID of the HCTL configuration interface.
*/
const TInt KHctlConfigInterfaceUid = 0x10274085;

/**
Mixin for the HCTL configuration interface.
*/
class MHctlConfigInterface
	{
public:
	/**
	Update the BAUD rate.
	@param aBaudRate New value for BAUD rate
	@return Standard error value.
	*/
	virtual TInt MhciUpdateBaudRate(TUint32 aBaudRate) = 0;
	};

#endif // HCTLCONFIGINTERFACE_H
