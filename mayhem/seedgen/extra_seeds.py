#!/usr/bin/env python3
"""Hand-built starter seeds for loaders that have no upstream saver (XM, MIDI): seedgen can't write
them. Usage: extra_seeds.py <out-dir-prefix>  (writes <prefix>/fuzz_load_song/seed_{xm,mid})."""
import os, struct, sys


def pad(b, n):
    return b[:n].ljust(n, b"\0")


def xm():
    chans, rows = 4, 8
    out = b"Extended Module: " + pad(b"seed", 20) + b"\x1a" + pad(b"seedgen", 20) + struct.pack("<H", 0x0104)
    hdr = struct.pack("<HHHHHHHH", 1, 0, chans, 1, 1, 1, 6, 125) + bytes(256)
    out += struct.pack("<I", 4 + len(hdr)) + hdr
    cells = b""
    for r in range(rows):
        for c in range(chans):
            cells += bytes([49 + r + c, 1, 0, 0, 0]) if (r + c) % 3 == 0 else bytes([0, 0, 0, 0, 0])
    out += struct.pack("<IBHH", 9, 0, rows, len(cells)) + cells
    n = 64
    data = bytes((i * 4) % 256 for i in range(n))
    delta = bytes((data[i] - (data[i - 1] if i else 0)) % 256 for i in range(n))
    ins = struct.pack("<I", 263) + pad(b"seed inst", 22) + b"\0" + struct.pack("<H", 1)
    ins += struct.pack("<I", 40) + bytes(96) + bytes(48) + bytes(48)
    ins += bytes([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]) + struct.pack("<H", 0) + bytes(22)
    ins = ins[:263]
    smp = struct.pack("<IIIBbBBbB", n, 0, n, 64, 0, 1, 128, 0, 0) + pad(b"seed sample", 22)
    return out + ins + smp + delta


def mid():
    trk = b""
    for i, note in enumerate([60, 64, 67, 72]):
        trk += bytes([0 if i == 0 else 48, 0x90, note, 100]) + bytes([48, 0x80, note, 0])
    trk += bytes([0, 0xFF, 0x2F, 0])
    return b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96) + b"MTrk" + struct.pack(">I", len(trk)) + trk


d = os.path.join(sys.argv[1], "fuzz_load_song")
os.makedirs(d, exist_ok=True)
for name, blob in (("xm", xm()), ("mid", mid())):
    open(os.path.join(d, "seed_" + name), "wb").write(blob)
    print(name, len(blob))
