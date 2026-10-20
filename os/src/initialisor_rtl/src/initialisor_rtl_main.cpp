// initialisor_rtl_main.cpp -- OURS (Bluetooth rung R3): the ECOM implementation table of initialisor_rtl.dll
/**
 @file
 @internalComponent
*/

#include <ecom/implementationproxy.h>
#include "rtlinitialisor.h"

const TImplementationProxy ImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(0xE7015733, CRtlInitialisor::NewL),
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
	return ImplementationTable;
	}
