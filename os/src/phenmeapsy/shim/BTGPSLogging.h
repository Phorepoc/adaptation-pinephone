// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// BTGPSLogging.h -- OURS: the real BTGPSFix / parser / buffer log through these macros (RFileLogger, flogger.lib) in a
// debug build. The serial NMEA PSY logs nothing from them: empty macros, found before the real header.
#ifndef BTGPSLOGGING_H
#define BTGPSLOGGING_H
#define CLEARTRACELOG
#define TRACETEXT( S1 )
#define TRACESTRING( S1 )
#define TRACESTRING2( S1, S2 )
#define TRACESTRING8_old( S1 )
#define TRACESTRING8( S1 )
#define TRACESTRING28( S1, S2 )
#define TRACESTRING38( S1, S2, S3 )
#endif
