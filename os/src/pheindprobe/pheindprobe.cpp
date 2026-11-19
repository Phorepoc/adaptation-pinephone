// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheindprobe -- OURS: the layout of the status pane's universal indicator pane, as AknIndicatorContainer::
// SizeChangedInNormalStatusPane reads it (AvKon/src/AknIndicatorContainer.cpp): TAknLayoutRect::LayoutRect(container rect,
// AknLayoutScalable_Avkon::indicator_pane_g1(variety)). The container is the 80 x 19 window measured at (261,0) of the
// 360 x 640 screen (the envelope is clipped on its right side: the slot is narrower than the icon). Lines: "PH: indprobe ...".
#include <e32base.h>
#include <e32debug.h>
#include <coemain.h>
#include <aknutils.h>
#include <aknlayoutscalable_avkon.cdl.h>

static void Rect(const char* aWhat, TInt aVariety, const TAknWindowLineLayout& aLine, const TRect& aParent)
	{
	TAknLayoutRect lr;
	lr.LayoutRect(aParent, aLine);
	const TRect r(lr.Rect());
	RDebug::Printf("PH: indprobe %s(%d) in %dx%d: line il=%d it=%d ir=%d ib=%d iW=%d iH=%d -> %d,%d-%d,%d (w %d h %d)",
		aWhat, aVariety, aParent.Width(), aParent.Height(), (TInt)aLine.il, (TInt)aLine.it, (TInt)aLine.ir, (TInt)aLine.ib,
		(TInt)aLine.iW, (TInt)aLine.iH, r.iTl.iX, r.iTl.iY, r.iBr.iX, r.iBr.iY, r.Width(), r.Height());
	}

static void ProbeL()
	{
	// A CCoeEnv is needed (without one the CDL engine panics "CdlEng 0" on the phone, boot 2795). Its ConstructL makes its OWN
	// cleanup stack: a CTrapCleanup of ours made it panic E32USER-CBase 66 (a push with no TRAP level, boot 2793), so main installs none.
	CCoeEnv* env = new (ELeave) CCoeEnv;
	env->ConstructL();
	const TSize scr(env->ScreenDevice()->SizeInPixels());
	RDebug::Printf("PH: indprobe screen %d x %d", scr.iWidth, scr.iHeight);
	const TRect pane(0, 0, 80, 19);
	for (TInt v = 0; v < 2; ++v)
		{
		TRAPD(e, Rect("indicator_pane_g1", v, AknLayoutScalable_Avkon::indicator_pane_g1(v).LayoutLine(), pane));
		if (e != KErrNone)
			RDebug::Printf("PH: indprobe indicator_pane_g1(%d) left %d", v, e);
		}
	for (TInt v = 0; v < 2; ++v)
		{
		TRAPD(e, Rect("uni_indicator_pane_g1", v, AknLayoutScalable_Avkon::uni_indicator_pane_g1(v).LayoutLine(), pane));
		if (e != KErrNone)
			RDebug::Printf("PH: indprobe uni_indicator_pane_g1(%d) left %d", v, e);
		}
	RDebug::Printf("PH: indprobe done");
	delete env;
	}

TInt E32Main()
	{
	TRAPD(err, ProbeL());
	if (err != KErrNone)
		RDebug::Printf("PH: indprobe failed %d", err);
	return err;
	}
