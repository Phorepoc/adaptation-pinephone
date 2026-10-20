# RTL8703B (RTL8723CS WiFi half) firmware

`rtw8703b_fw.bin` (20,290 bytes, firmware v11), `rtw8703b_wow_fw.bin` (23,074 bytes; the WoWLAN image -- rtw88 refuses
to power the chip on without it, `rtw_wait_firmware_completion`) and `LICENCE.rtlwifi_firmware.txt` are copied
**unmodified** from linux-firmware (`rtw88/rtw8703b_fw.bin`, `rtw88/rtw8703b_wow_fw.bin`, kernel.org `linux-firmware.git`, fetched 2026-10-10). Realtek's notice allows
redistribution of the firmware data "in hexadecimal or equivalent format, provided this copyright notice accompanies
it" -- the licence file is staged next to the firmware in the ROM (`Z:\resource\wlan\`, `tools/zdata_gen.py`).

Rules (the same as `../../bt/fw/`): ship as is, never disassemble or modify, **never into `deliverables/`**.

Consumers: the kernel's chip core reads it straight out of the XIP ROM (`src/phorepoc-arm/a64/wlan/a64wlan.cpp`) and
hands it to the vendored rtw88 (`request_firmware` in `shim_rt.cpp`); the OURS `wlanhwinit` will load it from user
side through the WHA front later.
