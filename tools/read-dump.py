#!/usr/bin/env python3
"""Validate a PMDMP01 support bundle and print a small JSON summary."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib


def decode(data):
    if len(data) != 964 or data[:8] != b'PMDMP01\n':
        raise ValueError('expected a 964-byte PMDMP01 support record')
    if zlib.crc32(data[:960]) != struct.unpack_from('<I', data, 960)[0]:
        raise ValueError('CRC32 mismatch')
    seg, length = struct.unpack_from('<HH', data, 8)
    if not (0xC000 <= seg <= 0xE800 and seg % 0x80 == 0 and length == 768):
        raise ValueError('unsupported address/length')
    if data[12] > 1 or any(data[13:32]):
        raise ValueError('unsupported flags/reserved fields')
    rom, bios, cfg = data[32:160], data[160:192], data[192:960]
    if (rom[:3] != b'\x55\xaa\x20' or rom[6:12] != b'PMBIOS' or
            b'(Date 2026-06-16' not in rom or bios[0] != 0x12 or bios[27] != 11):
        raise ValueError('unsupported firmware profile')
    return {'format': 'PMDMP01', 'source': 'fixture' if data[12] else 'hardware',
            'rom_segment': '%04X' % seg, 'board_id': bios[27],
            'bios_starts': bios[2], 'configuration_bytes': length,
            'config_sha256': hashlib.sha256(cfg).hexdigest(),
            'nic_irq': cfg[215], 'nic_port': '0x%03X' % struct.unpack_from('<H',cfg,216)[0],
            'crc32_valid': True}


if __name__ == '__main__':
    try:
        if len(sys.argv) != 2:
            raise ValueError('usage: read-dump.py SUPPORT_FILE')
        print(json.dumps(decode(Path(sys.argv[1]).read_bytes()), indent=2))
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
