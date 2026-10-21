// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// wlanhwinit.cpp -- OURS (2026-10-10, PinePhone WiFi W3): CWlanHwInit, the WLAN hardware-initialisation data the
// engine (wlmserver) asks for before it loads the drivers. Shape = the 2010 adapt stub (wlanhwinit_stub); the one
// real answer is the MAC address: the RTL8723CS efuse, read by the kernel's rtw88 probe at boot and handed out by the
// a64 power HAL (a64_glue.cpp, function 0x574C). No init/firmware blobs: our PDD loads the firmware from Z: itself.
#include <e32base.h>
#include <e32debug.h>
#include <e32svr.h>
#include <u32std.h>
#include <wlanhwinit.h>

const TInt KA64PowerHalWlanMac = 0x574C;

CWlanHwInit::CWlanHwInit() : iMain( NULL )
    {
    }

void CWlanHwInit::ConstructL()
    {
    }

EXPORT_C CWlanHwInit* CWlanHwInit::NewL()
    {
    CWlanHwInit* self = new( ELeave ) CWlanHwInit;
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop();
    return self;
    }

EXPORT_C CWlanHwInit::~CWlanHwInit()
    {
    }

EXPORT_C void CWlanHwInit::GetHwInitData( const TUint8** aInitData, TUint& aInitLength, const TUint8** aFwData, TUint& aFwLength )
    {
    *aInitData = NULL;
    aInitLength = 0;
    *aFwData = NULL;
    aFwLength = 0;
    }

EXPORT_C TInt CWlanHwInit::GetMacAddress( TMacAddr& aMacAddress )
    {
    TBuf8<6> mac;
    const TInt r = UserSvr::HalFunction( EHalGroupPower, KA64PowerHalWlanMac, &mac, 0 );
    if ( r != KErrNone || mac.Length() != 6 )
        {
        RDebug::Printf( "[wlanhwinit] no MAC from the kernel (r=%d): the chip did not probe", r );
        aMacAddress.iMacAddress[0] = 0x00;
        aMacAddress.iMacAddress[1] = 0xE0;
        aMacAddress.iMacAddress[2] = 0xDE;
        aMacAddress.iMacAddress[3] = 0xAD;
        aMacAddress.iMacAddress[4] = 0xBE;
        aMacAddress.iMacAddress[5] = 0xEF;
        return KErrNone;
        }
    for ( TInt i = 0; i < 6; ++i )
        aMacAddress.iMacAddress[i] = mac[i];
    RDebug::Printf( "[wlanhwinit] MAC %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    return KErrNone;
    }

EXPORT_C void CWlanHwInit::GetHwTestInitData( const TUint8** aInitData, TUint& aInitLength, const TUint8** aFwData, TUint& aFwLength )
    {
    *aInitData = NULL;
    aInitLength = 0;
    *aFwData = NULL;
    aFwLength = 0;
    }

EXPORT_C TInt CWlanHwInit::GetHwTestData( TUint /*aId*/, TDes8& /*aData*/ )
    {
    return KErrNotSupported;
    }

EXPORT_C TInt CWlanHwInit::SetHwTestData( TUint /*aId*/, TDesC8& /*aData*/ )
    {
    return KErrNotSupported;
    }
