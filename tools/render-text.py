#!/usr/bin/env python3
"""Render PPC512 mode-3 video RAM using its captured BIOS 8x8 font.

No terminal output is synthesized: cells and glyphs come from hardware reads.
The 200 scan lines are doubled vertically for the CGA display aspect ratio.
"""
import pathlib
import struct
import sys
import zlib

cells = pathlib.Path(sys.argv[1]).read_bytes()
font = pathlib.Path(sys.argv[2]).read_bytes()
if len(cells) != 4000 or len(font) != 1024:
    raise SystemExit('Expected 80x25 character/attribute pairs and 128 8x8 glyphs')
if any(cells[i] >= 128 for i in range(0, 4000, 2)):
    raise SystemExit('Capture uses extended characters absent from the ROM font')

palette = [(0,0,0), (0,0,170), (0,170,0), (0,170,170),
           (170,0,0), (170,0,170), (170,85,0), (170,170,170),
           (85,85,85), (85,85,255), (85,255,85), (85,255,255),
           (255,85,85), (255,85,255), (255,255,85), (255,255,255)]
rows = bytearray()
for y in range(200):
    row = bytearray(b'\0')
    for x in range(80):
        off = ((y // 8) * 80 + x) * 2
        char, attr = cells[off:off+2]
        glyph = font[char * 8 + y % 8]
        for bit in range(8):
            color = attr & 15 if glyph & (128 >> bit) else (attr >> 4) & 7
            row.extend(palette[color])
    rows.extend(row)
    rows.extend(row)

def chunk(kind, payload):
    return (struct.pack('>I', len(payload)) + kind + payload +
            struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))

png = b'\x89PNG\r\n\x1a\n'
png += chunk(b'IHDR', struct.pack('>IIBBBBB', 640, 400, 8, 2, 0, 0, 0))
png += chunk(b'IDAT', zlib.compress(rows))
png += chunk(b'IEND', b'')
pathlib.Path(sys.argv[3]).write_bytes(png)
for y in range(25):
    print(bytes(cells[y*160:y*160+160:2]).decode('ascii').rstrip())
