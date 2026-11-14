// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phedrives.cpp -- OURS: drive report for PhoneLab ("PH: drives" lines, forwarded by the kernel seam).
// For each mounted drive: media type, file system name, size and free space (MB). Then on C: and E: a 4 MB file is
// written, read back and compared, timed, and deleted.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

static void RoundTrip(RFs& aFs, TInt aDrive)
	{
	TBuf<32> name;
	name.Format(_L("%c:\\phedrives.tmp"), 'A' + aDrive);
	const TInt KChunk = 64 * 1024, KChunks = 64;		// 4 MB
	HBufC8* buf = HBufC8::New(KChunk);
	HBufC8* back = HBufC8::New(KChunk);
	if (!buf || !back)
		{
		delete buf; delete back;
		RDebug::Printf("PH: drives %c: round trip: no memory", 'A' + aDrive);
		return;
		}
	TPtr8 w = buf->Des(), r = back->Des();
	w.SetLength(KChunk);
	for (TInt i = 0; i < KChunk; ++i)
		w[i] = (TUint8)(i * 7 + aDrive);
	RFile f;
	TInt err = f.Replace(aFs, name, EFileWrite);
	const TUint32 t0 = User::NTickCount();
	for (TInt c = 0; err == KErrNone && c < KChunks; ++c)
		err = f.Write(w);
	if (err == KErrNone)
		err = f.Flush();
	f.Close();
	const TUint32 t1 = User::NTickCount();
	TInt bad = 0;
	if (err == KErrNone)
		err = f.Open(aFs, name, EFileRead);
	for (TInt c = 0; err == KErrNone && c < KChunks; ++c)
		{
		err = f.Read(r, KChunk);
		if (err == KErrNone && r != w)
			{
			if (bad++ == 0)
				{
				TInt at = 0;
				const TInt n = r.Length() < w.Length() ? r.Length() : w.Length();
				while (at < n && r[at] == w[at])
					++at;
				RDebug::Printf("PH: drives %c: chunk %d differs: read len %d, first diff at %d: read %02x want %02x",
					'A' + aDrive, c, r.Length(), at, at < r.Length() ? r[at] : 0, at < w.Length() ? w[at] : 0);
				}
			}
		}
	f.Close();
	const TUint32 t2 = User::NTickCount();
	const TInt del = aFs.Delete(name);
	RDebug::Printf("PH: drives %c: 4 MB round trip err=%d bad chunks=%d write %d ms read %d ms delete=%d", 'A' + aDrive, err,
		bad, (TInt)(t1 - t0), (TInt)(t2 - t1), del);
	delete buf;
	delete back;
	}

TInt E32Main()
	{
	RFs fs;
	if (fs.Connect() != KErrNone)
		{
		RDebug::Printf("PH: drives: no file server");
		return 0;
		}
	TDriveList list;
	fs.DriveList(list);
	for (TInt d = 0; d < KMaxDrives; ++d)
		{
		if (!list[d])
			continue;
		TVolumeInfo v;
		const TInt r = fs.Volume(v, d);
		TFullName fsName;
		fs.FileSystemName(fsName, d);
		TBuf8<32> fs8;
		fs8.Copy(fsName.Left(30));
		RDebug::Printf("PH: drives %c: r=%d media=%d fs=%S size=%Ld MB free=%Ld MB att=%x", 'A' + d, r,
			(TInt)v.iDrive.iType, &fs8, v.iSize >> 20, v.iFree >> 20, v.iDrive.iDriveAtt);
		}
	RoundTrip(fs, EDriveC);
	RoundTrip(fs, EDriveE);
	fs.Close();
	RDebug::Printf("PH: drives done");
	return 0;
	}
