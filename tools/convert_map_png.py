#!/usr/bin/env python3
"""Convert a web-build map's height / substrate PNGs to raw files the UE build reads directly.

  python3 tools/convert_map_png.py <web repo>/public/data/maps/hashirimizu Content/Data/maps/hashirimizu

height.png (RGBA8, 16-bit height in R = high byte, G = low byte) -> height.u16 (little-endian uint16, row-major,
row 0 = north edge, column 0 = west edge). substrate.png (index in R) -> substrate.u8. The metres the 16 bits span
(min_tp_m / max_tp_m) stay in the map JSON. Raw files rather than PNGs so the editor never offers to import them.
Standard library only.
"""
import struct
import sys
import zlib
from pathlib import Path


def read_rgba_png(path):
    data = Path(path).read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path}: not a PNG'
    pos, idat, width, height = 8, b'', 0, 0
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            width, height, depth, color = struct.unpack('>IIBB', chunk[:10])
            assert depth == 8 and color == 6, f'{path}: expected 8-bit RGBA, got depth {depth} color type {color}'
        elif kind == b'IDAT':
            idat += chunk
    raw, bpp = zlib.decompress(idat), 4
    stride, out, prev, i = width * bpp, bytearray(), bytearray(width * bpp), 0
    for _ in range(height):
        kind, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if kind == 1:
                line[x] = (line[x] + a) & 255
            elif kind == 2:
                line[x] = (line[x] + b) & 255
            elif kind == 3:
                line[x] = (line[x] + (a + b) // 2) & 255
            elif kind == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line
        prev = line
    return width, height, bytes(out)


def main(src, dst):
    src, dst = Path(src), Path(dst)
    dst.mkdir(parents=True, exist_ok=True)
    w, h, px = read_rgba_png(src / 'height.png')
    (dst / 'height.u16').write_bytes(b''.join(struct.pack('<H', (px[k * 4] << 8) | px[k * 4 + 1]) for k in range(w * h)))
    print(f'height.u16: {w} x {h}')
    if (src / 'substrate.png').exists():
        w2, h2, sp = read_rgba_png(src / 'substrate.png')
        assert (w2, h2) == (w, h), 'substrate and height sizes differ'
        (dst / 'substrate.u8').write_bytes(bytes(sp[k * 4] for k in range(w * h)))
        print(f'substrate.u8: {w} x {h}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
