# CommsDat for the PinePhone board

`cccccc00.cre` is the CommsDat database a `BOOT=ssm` ROM ships as `Z:\private\10202be9\cccccc00.cre`: our network
configuration (Ethernet with DHCP (the phone joins WLAN at run time, wlanjoin adds its access point), the S60 tables and the "Internet" destination) run through the 2010
release's CED on a booted image, then taken off its disk. A phone ROM carries CED's output; it never runs CED itself.

The ROM recipe uses `<build>/commsdat/cccccc00.cre` when one was made for the build, else this file. Regenerate it when
the configuration changes (a BOOT=bootui ROM with networking runs CED + cmsetup at boot; copy the file off its C: disk).
No network names or keys are in it.
