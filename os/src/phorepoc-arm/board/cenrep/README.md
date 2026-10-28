# Board-owned CenRep overrides (PinePhone / BOARD=a64)

`tools/make_phorepoc_rom.sh` stages every `*.txt` here as `private\10202be9\<uid>.txt` AFTER the generated
(`.cenrep-gen`, from the crml files) and the listed (`tools/kernel/cenrep_files.txt`) repositories, and BEFORE the
per-run `CENREP_EXTRA` overrides -- only on the ARM ROM (`ROM_ARCH=arm`). Each file is a whole repository (the
cenrep text format), copied from the generated one and edited; say why in this table.

**Encoding: UTF-16LE with a BOM and CRLF line ends**, like `.cenrep-gen/*.txt` and `tools/kernel/cenrep/*.txt`. A
plain ASCII/LF file is "corrupt" to the CenRep server (`CRepository::NewL` leaves -20 and the owner falls back to its
compiled-in defaults -- boot 209, 2026-10-10). Re-encode with:
`py -c "p='<file>';t=open(p,'rb').read().decode('ascii').replace('\r\n','\n').replace('\n','\r\n');open(p,'wb').write(b'\xff\xfe'+t.encode('utf-16-le'))"`
and check with `file <file>` ("Unicode text, UTF-16, little-endian").

| uid | what | why ours differs from the generated defaults |
|---|---|---|
| `10282df0` | sensor settings (gssensorplugin's `sensorplugin_10282DF0.crml`; the 2010 defaults are all 0 = sensors off) | sensors on (key 1 = 1) and turning control = auto-rotate (key 3 = 4), as a shipping S60 phone sets them; key 3 = 4 (auto-rotate; was 0 until the rotated frame (S2b, 2026-10-15) and the MPU-6050 upright sign were sorted: measured 2026-10-17 boot 868, phone upright on a ~45-60 deg stand: y=+14138 z=+9446, orientation 1 = up -> upright = +Y as coded). NOTE: a phone whose C: holds a persisted 10282df0 keeps its old value (Control Panel > turning control). |
| `10282f0f` | the orientation SSY's threshold/timer table (`internalorientationcrkeys.h`) | a shipping S60 phone's 60 values (the 2010 confml defaults are 5/150, 30/300 ...); a starting point, to be retuned on the PinePhone |
| `1020507e` | sensor server settings (`sensrvprivatecrkeys.h`) | key 2 = SSY thread stack **16384** (2010: 4096). Our GCC-built ECOM load path (`REComSession::CreateImplementationL` -> `RLibrary::InitL`) overflowed the 4 KB stack of the SSY thread on boot 207 (KERN-EXEC 3, far = sp + 4, "stack overflow into the guard page"); RVCT frames fit, GCC's do not. The rest = a shipping S60 phone's values (timeout 60 s, heap max 65535, inactivity 60 s). |
| `101f877c` | personalization settings (`SettingsInternalCRKeys.h`; generated from the crml) | key 0x17 `KSettingsScreensaverTimeoutItemVisibility` = **1** (generated: 0): Control Panel > Display shows the Screen saver time-out item, as S60 phones do (user, 2026-10-15). The rest = the generated defaults. |
| `e70157b0` | OURS: the power key options (SysAp delta toolchain/shims/sysap_gcc/SysApAppUi.cpp) | key 1 = SHORT press, key 2 = LONG press: 0 = power menu, 1 = lock / unlock, 2 = switch off. Defaults short = lock/unlock, long = menu (user, 2026-10-15: no lock switch on the PinePhone). The Control Panel page writes it (next). |

- `2000b494.txt` (tactile feedback): key 0x90 (KTactileHapticsPlugin) = 0x2001CB9B, the vibra player (HWRM vibra ->
  the motor) instead of the haptics-chip player 0x2001CB99 -- the PinePhone has a plain vibration motor.
  Keys 0x25-0x2e (vibra pulse lengths, ms): basic 60-80, sensitive 40 instead of 15-50 / 10: the motor does not
  spin up in 15 ms (a touch gave no felt buzz, 2026-10-28).
