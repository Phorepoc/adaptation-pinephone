# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-1.0

"""components_pinephone -- the adaptation-pinephone components (merged into core's registry when this adaptation is built:
PHOREPOC_ADAPTATIONS, see the pdk's tools/emitter/paths.py). Paths are relative to the merged OS tree."""
import os

from paths import OS_ROOT  # noqa: F401

COMPONENTS = {
    "btprobe": dict(mmp=os.path.join(OS_ROOT, "src/btprobe/btprobe.mmp"), test=True),
    # CPIN/CSQ/COPS, AT+QPOWD; "PH: modem" lines. ARM_VFP=1 ./ph arm phemodemtest; MODEM=1 SSM_TEST=phemodemtest
    "phemodemtest": dict(mmp=os.path.join(OS_ROOT, "src/phemodemtest/phemodemtest.mmp"), test=True),
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
}

# core components that get this board's data (core registry: EXTEND)
EXTEND = {
    # the download app's device identity (board + architecture): it shows only what fits this board
    "pheget": dict(rom_data=[r"resource\phorepoc\device.txt=data/phorepoc/device_pinephone.txt"]),
}
