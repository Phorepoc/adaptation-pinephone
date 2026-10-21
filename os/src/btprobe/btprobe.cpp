// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// btprobe.cpp -- OURS (2026-10-09, Bluetooth rung R1): does the RTL8723CS answer on UART1? Opens the serial port
// (RBusDevComm unit 1 = the "Comm.A64" PDD) at 115200 8E1 with a 0xC0 terminator, powers the chip through the port's
// DTR convention (enable PL4 + device-wake PH6; Linux hci_h5 timing: off 100 ms, on 500 ms), then runs the H5
// three-wire link establishment by hand (research/bluetooth-linux.md 1.1): SYNC every 100 ms until SYNC_RSP, answer
// the chip's SYNC with SYNC_RSP, CONFIG until CONFIG_RSP. Everything received is logged in hex (A64_RDEBUG_LOG).
// SSM_TEST=btprobe stages it as ssmtest.exe, started by the SSM's non-critical list.
#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>

_LIT(KLdd, "ECOMM");

static void Hex(const TDesC8& aData, const char* aTag)
	{
	TBuf8<160> s;
	for (TInt i = 0; i < aData.Length() && s.Length() < 150; ++i)
		s.AppendFormat(_L8("%02x "), aData[i]);
	RDebug::Printf("[btprobe] %s (%d): %S", aTag, aData.Length(), &s);
	}

static TInt ReadFor(RBusDevComm& aPort, TDes8& aBuf, TInt aUs)
	{
	TRequestStatus rs, ts;
	RTimer t;
	t.CreateLocal();
	aBuf.Zero();
	aPort.ReadOneOrMore(rs, aBuf);
	t.After(ts, aUs);
	User::WaitForRequest(rs, ts);
	if (rs == KRequestPending)
		{
		aPort.ReadCancel();
		User::WaitForRequest(rs);
		}
	else
		{
		t.Cancel();
		User::WaitForRequest(ts);
		}
	t.Close();
	return rs.Int() == KErrNone ? aBuf.Length() : rs.Int();
	}

static void WriteSync(RBusDevComm& aPort, const TDesC8& aFrame)
	{
	TRequestStatus ws;
	aPort.Write(ws, aFrame);
	User::WaitForRequest(ws);
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[btprobe] start");
	TInt r = User::LoadLogicalDevice(KLdd);
	RDebug::Printf("[btprobe] LoadLogicalDevice(ECOMM) -> %d", r);
	RBusDevComm port;
	r = port.Open(1);
	RDebug::Printf("[btprobe] RBusDevComm::Open(1) -> %d", r);
	if (r != KErrNone)
		return r;
	TCommConfig cfg;
	port.Config(cfg);
	cfg().iRate = EBps115200;
	cfg().iDataBits = EData8;
	cfg().iStopBits = EStop1;
	cfg().iParity = EParityEven;
	cfg().iHandshake = KConfigFreeRTS | KConfigFreeDTR;
	cfg().iTerminatorCount = 1;
	cfg().iTerminator[0] = 0xC0;
	r = port.SetConfig(cfg);
	RDebug::Printf("[btprobe] SetConfig 115200 8E1 term C0 -> %d, signals %x", r, port.Signals());
	// the power sequence (the port's DTR = BT enable + device wake)
	port.SetSignals(0, KSignalDTR);
	User::After(100000);
	port.SetSignals(KSignalDTR, 0);
	User::After(500000);
	RDebug::Printf("[btprobe] chip enabled; signals %x", port.Signals());
	// H5 link establishment (SLIP-framed link-control packets, type 15, unreliable)
	_LIT8(KSync,    "\xC0\x00\x2F\x00\xD0\x01\x7E\xC0");
	_LIT8(KSyncRsp, "\xC0\x00\x2F\x00\xD0\x02\x7D\xC0");
	_LIT8(KConfig,  "\xC0\x00\x3F\x00\xDB\xDC\x03\xFC\x14\xC0");
	_LIT8(KConfRsp, "\xC0\x00\x2F\x00\xD0\x04\x7B\xC0");
	TBuf8<64> buf;
	TBuf8<64> frame;											// the SLIP frame being assembled (unescaped)
	TBool esc = EFalse;
	TInt state = 0;												// 0 uninitialised, 1 initialised, 2 active
	TInt rxTotal = 0, loops = 0, field = -1;
	for (loops = 0; loops < 80 && state < 2; ++loops)			// ~8 s
		{
		WriteSync(port, state == 0 ? KSync() : KConfig());
		const TInt n = ReadFor(port, buf, 100000);
		if (n > 0)
			{
			rxTotal += n;
			Hex(buf, "rx");
			// SLIP: 0xC0 ends a frame (an empty one is a delimiter pair); DB DC = C0, DB DD = DB. A link-control
			// packet is header {seq/ack, type 15 | len, len hi, ~sum} + 2..3 payload bytes
			for (TInt i = 0; i < buf.Length(); ++i)
				{
				const TUint8 c = buf[i];
				if (c == 0xC0)
					{
					if (frame.Length() >= 6 && (frame[1] & 0x0F) == 0x0F)
						{
						const TUint8 a = frame[4], b = frame[5];
						if (a == 0x01 && b == 0x7E) { RDebug::Printf("[btprobe] got SYNC -> SYNC_RSP"); WriteSync(port, KSyncRsp()); }
						else if (a == 0x02 && b == 0x7D) { RDebug::Printf("[btprobe] got SYNC_RSP -> initialised"); if (state < 1) state = 1; }
						else if (a == 0x03 && b == 0xFC) { RDebug::Printf("[btprobe] got CONFIG (field %02x) -> CONFIG_RSP", frame.Length() > 6 ? frame[6] : 0); WriteSync(port, KConfRsp()); }
						else if (a == 0x04 && b == 0x7B) { field = frame.Length() > 6 ? frame[6] : -1; RDebug::Printf("[btprobe] got CONFIG_RSP (field %02x: window %d, crc %d) -> ACTIVE", field & 0xFF, field & 7, (field >> 4) & 1); state = 2; }
						else RDebug::Printf("[btprobe] link msg %02x %02x", a, b);
						}
					else if (frame.Length())
						Hex(frame, "frame (not link)");
					frame.Zero();
					esc = EFalse;
					continue;
					}
				if (esc)
					{
					esc = EFalse;
					if (frame.Length() < frame.MaxLength()) frame.Append(c == 0xDC ? 0xC0 : (c == 0xDD ? 0xDB : c));
					}
				else if (c == 0xDB)
					esc = ETrue;
				else if (frame.Length() < frame.MaxLength())
					frame.Append(c);
				}
			}
		else if (n < 0 && n != KErrCancel)
			RDebug::Printf("[btprobe] read error %d", n);
		}
	RDebug::Printf("[btprobe] DONE state=%d rx bytes=%d loops=%d signals=%x", state, rxTotal, loops, port.Signals());
	port.SetSignals(0, KSignalDTR);
	port.Close();
	return state == 2 ? 0 : 1;
	}
