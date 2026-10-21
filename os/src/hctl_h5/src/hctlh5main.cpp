// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// hctlh5main.cpp -- OURS (Bluetooth rung R2): the ECOM implementation table of hctl_h5.dll
/**
 @file
 @internalComponent
*/

#include <ecom/implementationproxy.h>
#include "hctlh5.h"

const TImplementationProxy ImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(0xE7015731, CHCTLH5::NewL),
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
	return ImplementationTable;
	}
