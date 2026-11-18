# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

"""components_pinephone -- the adaptation-pinephone components (merged into core's registry when this adaptation is built:
PHOREPOC_ADAPTATIONS, see the pdk's tools/emitter/paths.py). Paths are relative to the merged OS tree."""
import os

from paths import OS_ROOT  # noqa: F401

_L = "oss.FCL.sf.os.lbs/"
# the location stack ships only in an image built with SSM_TEST=phelbstest (build_env cannot carry a comma list: ROM_TAIL)
_LBS_TEST = os.environ.get("SSM_TEST") != "phelbstest"
_BT = "baseline/symbian-2010/oss.FCL.sf.os.lbs/datasourcemodules/bluetoothgpspositioningmodule/btgpspsy/"

COMPONENTS = {
    "btprobe": dict(mmp=os.path.join(OS_ROOT, "src/btprobe/btprobe.mmp"), test=True),
    # CPIN/CSQ/COPS, AT+QPOWD; "PH: modem" lines. ARM_VFP=1 ./ph arm phemodemtest; MODEM=1 SSM_TEST=phemodemtest
    "phemodemtest": dict(mmp=os.path.join(OS_ROOT, "src/phemodemtest/phemodemtest.mmp"), test=True),
    # modem step 3: the EG25-G AT port over USB (COMM::8..15 via RComm); MODEM=1 A64_USBHOST=1; started by hand
    "phemodemusb": dict(mmp=os.path.join(OS_ROOT, "src/phemodemusb/phemodemusb.mmp"), test=True),
    # GNSS rung 1: AT+QGPS=1 over COMM::11, 60 s of NMEA from COMM::9 counted by sentence type (no coordinates), AT+QGPSEND; MODEM=1 A64_USBHOST=1 SSM_TEST=phegnsstest
    "phegnsstest": dict(mmp=os.path.join(OS_ROOT, "src/phegnsstest/phegnsstest.mmp"), test=True),
    # Bluetooth rung R2: powers the 2010 stack on (RBTPowerControl -> bt_v2.prt -> corehci -> hctl_h5) and reads BD_ADDR
    "btpower": dict(mmp=os.path.join(OS_ROOT, "src/btpower/btpower.mmp"), test=True),
    # Bluetooth rung R4: Bluetooth on through the S60 engine (CBTEngSettings -> btengsrv -> the stack), visible, named
    "btengon": dict(mmp=os.path.join(OS_ROOT, "src/btengon/btengon.mmp"), test=True),
    "senstest": dict(mmp=os.path.join(OS_ROOT, "src/senstest/senstest.mmp"), test=True),
    "phedrives": dict(mmp=os.path.join(OS_ROOT, "src/phedrives/phedrives.mmp"), test=True),
    # PDD, the networks listed; SSM_TEST=wlantest
    "wlantest": dict(mmp=os.path.join(OS_ROOT, "src/wlantest/wlantest.mmp"), test=True),
    # RConnection::Start, DHCP, DNS + HTTP); SSM_TEST=wlanjoin; the network from C:\wlanjoin.txt "<ssid>|<psk>"
    "wlanjoin": dict(mmp=os.path.join(OS_ROOT, "src/wlanjoin/wlanjoin.mmp"), test=True),
    # without it); the release's stub for now, OURS (firmware + MAC) with the chip core
    # HAL) -- wlmserver refuses the stub's 00:E0:DE:AD:BE:EF. The 2010 stub: oss.FCL.sf.adapt.stubs/wlanhwinit_stub
    "wlanhwinit": dict(mmp=os.path.join(OS_ROOT, "src/wlanhwinit/group/wlanhwinit.mmp")),
    # OURS: the H5 (three-wire UART) HCTL for the RTL8723CS on UART1, forked from hctl_bcsp (src/hctl_h5)
    "hctl_h5": dict(mmp=os.path.join(OS_ROOT, "src/hctl_h5/group/hctl_h5.mmp")),
    # OURS (R3): the Realtek initialisor = the firmware patch download + the 1.5 Mbaud switch (src/initialisor_rtl)
    "initialisor_rtl": dict(mmp=os.path.join(OS_ROOT, "src/initialisor_rtl/group/initialisor_rtl.mmp")),
    # ---- GNSS rung 2: the 2010 location stack (oss.FCL.sf.os.lbs, the real mmps) + a serial NMEA PSY ----------------------
    # All test components unless SSM_TEST=phelbstest (a variant): the release image is unchanged. The
    # modules repository (CenRep 0x101f500c) lists the PSY; the PSY's own repository (its implementation UID) holds its
    # properties; the LBS settings repositories are the real binary .cre files of the package.
    "eposserver": dict(mmp=_L + "locationrequestmgmt/locationserver/group/EPos_Server.mmp", test=_LBS_TEST),
    "epospluginfw": dict(mmp=_L + "datasourceadaptation/gpsdatasourceadaptation/group/EPos_PluginFw.mmp", test=_LBS_TEST,
                         x32_flags=["-DPORT_FOFF_DYNAMIC"]),   # CPositioner::NewL: static_cast<TInt32> _FOFF(..) (the run-time pointer form)
    "eposmodset": dict(mmp=_L + "locationmgmt/locationcore/lbslocsettings/group/EPos_LocationSettings.mmp", test=_LBS_TEST,
                       rom_data=[r"private\10202be9\101f500c.txt=data/lbs/101f500c.txt",
                                 r"private\10202be9\e7016c20.txt=data/lbs/e7016c20.txt"]),
    "eposutils": dict(mmp=_L + "locationmgmt/locationcore/lbsmlfwutilities/group/EPos_Utils.mmp", test=_LBS_TEST),
    "lbsadmin": dict(mmp=_L + "locationmgmt/locationcore/group/lbsadmin.mmp", test=_LBS_TEST,
                     rom_data=[r"private\10202be9\1028224b.cre=baseline/symbian-2010/" + _L + "locationmgmt/locationcore/group/1028224B.cre"]),
    "lbsassistancedata": dict(mmp=_L + "locationmgmt/locationcore/LbsAssistanceData/group/LbsAssistanceData.mmp", test=_LBS_TEST),
    "lbscommoninternaldatatypes": dict(mmp=_L + "locationmgmt/locationcore/LbsCommonInternalDataTypes/group/lbscommoninternaldatatypes.mmp", test=_LBS_TEST),
    "lbsinternalapi": dict(mmp=_L + "locationmgmt/locationcore/LbsInternalApi/group/LbsInternalApi.mmp", test=_LBS_TEST,
                           rom_data=[r"private\10202be9\10282266.cre=baseline/symbian-2010/" + _L + "locationmgmt/locationcore/lbsroot/group/10282266.cre"]),
    "lbslocationinfodatatypes": dict(mmp=_L + "locationmgmt/locmonitor/lbslocmonitorapis/ConversionDataTypes/group/lbslocationinfodatatypes.mmp", test=_LBS_TEST),
    "lbsloccommon": dict(mmp=_L + "locationmgmt/locationcore/LbsLocCommon/group/LbsLocCommon.mmp", test=_LBS_TEST),
    "lbslocdatasource": dict(mmp=_L + "locationmgmt/locationcore/LbsLocDataSource/group/LbsLocDataSource.mmp", test=_LBS_TEST),
    "lbslocmonclient": dict(mmp=_L + "locationmgmt/locmonitor/lbslocmonitorapis/CommonClientServerApi/group/lbslocmonclient.mmp", test=_LBS_TEST),
    "lbsnetinternalapi": dict(mmp=_L + "locationmgmt/locationcore/LbsNetInternalApi/group/lbsnetinternalapi.mmp", test=_LBS_TEST),
    "lbsnetprotocol": dict(mmp=_L + "locationmgmt/locationcore/LbsNetProtocol/group/lbsnetprotocol.mmp", test=_LBS_TEST),
    "lbsnetregstatusint": dict(mmp=_L + "locationmgmt/locationcore/LbsNetRegStatusInt/group/lbsnetregstatusint.mmp", test=_LBS_TEST),
    "lbspartnercommon": dict(mmp=_L + "locationmgmt/locationcore/LbsPartnerCommon/group/lbspartnercommon.mmp", test=_LBS_TEST),
    "lbsrootapi": dict(mmp=_L + "locationmgmt/locationcore/lbsrootapi/group/lbsrootapi.mmp", test=_LBS_TEST),
    # OURS: the serial NMEA PSY = the Bluetooth GPS PSY's real NMEA parser / fix / buffer with the Bluetooth transport
    # replaced by RComm on the modem's NMEA port, GNSS on/off over the second AT port (src/phenmeapsy)
    "phenmeapsy": dict(mmp=os.path.join(OS_ROOT, "src/phenmeapsy/phenmeapsy.mmp"), test=_LBS_TEST, arm_incs_first=True,
                       incs_first=["src/phenmeapsy/shim", _BT + "inc", _BT + "inc/Nmea", _BT + "inc/Utils", _BT + "inc/RequestHandler",
                                   _BT + "inc/Settings"],
                       extra=[_BT + "src/Nmea/BTGPSNmeaParser.cpp", _BT + "src/BTGPSFix.cpp", _BT + "src/Utils/BTGPSNmeaBuffer.cpp"]),
    # the test: RPositionServer -> RPositioner on the serial PSY; SSM_TEST=phelbstest; module status + satellite counts, no coordinates
    "phelbstest": dict(mmp=os.path.join(OS_ROOT, "src/phelbstest/phelbstest.mmp"), test=True),
}

# core components that get this board's data (core registry: EXTEND)
EXTEND = {
    # the download app's device identity (board + architecture): it shows only what fits this board (the catalogue layer)
    "catalogue_get": dict(rom_data=[r"resource\phorepoc\device.txt=data/phorepoc/device_pinephone.txt"]),
    # pheltsy, the AT engine of the licensee TSY (core toolchain/shims/licenseetsy_board/pheltsy_at.h): compiled into this
    # board's licenseetsy.dll. It takes over from the fake modem only when the modem power unit (Comm unit 3, the MODEM=1
    # kernel) opens; otherwise the scripted values stay
    "licenseetsy": dict(defs=["PHE_LTSY"]),
}
