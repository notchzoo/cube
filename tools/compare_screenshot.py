#!/usr/bin/env python3
"""Compare two 24-bit BMP screenshots pixel by pixel (rendering regression).

usage:
    python tools/compare_screenshot.py <baseline.bmp> <candidate.bmp> [--tolerance N]

exit: 0 = match within tolerance, 1 = regression, 2 = bad input.
No third-party dependency: only uncompressed 24-bit BMP (what --screenshot writes).
"""
import struct
import sys


def read_bmp24(path):
    with open(path, "rb") as stream:
        data = stream.read()
    if len(data) < 54 or data[0:2] != b"BM":
        raise ValueError(f"{path}: not a BMP file")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    if bits != 24:
        raise ValueError(f"{path}: expected 24 bits per pixel, got {bits}")
    flipped = height < 0
    height = abs(height)
    row_bytes = (width * 3 + 3) & ~3
    rows = []
    for y in range(height):
        start = pixel_offset + y * row_bytes
        rows.append(data[start:start + row_bytes])
    if flipped:
        rows.reverse()
    return width, height, row_bytes, rows


def compare(baseline, candidate, tolerance):
    w1, h1, rb1, rows1 = read_bmp24(baseline)
    w2, h2, rb2, rows2 = read_bmp24(candidate)
    if (w1, h1) != (w2, h2):
        print(f"size mismatch: {baseline} {w1}x{h1} vs {candidate} {w2}x{h2}")
        return 2
    differing = 0
    max_delta = 0
    for y in range(h1):
        a = rows1[y]
        b = rows2[y]
        for x in range(w1):
            i = x * 3
            delta = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
            if delta > 0:
                differing += 1
                if delta > max_delta:
                    max_delta = delta
    total = w1 * h1
    print(f"size {w1}x{h1} | differing pixels {differing}/{total} ({differing / total * 100:.3f}%) | max channel delta {max_delta}")
    return 0 if max_delta <= tolerance else 1


def main(argv):
    args = [a for a in argv[1:]]
    tolerance = 0
    if "--tolerance" in args:
        index = args.index("--tolerance")
        tolerance = int(args[index + 1])
        del args[index:index + 2]
    if len(args) != 2:
        print(__doc__)
        return 2
    try:
        return compare(args[0], args[1], tolerance)
    except (OSError, ValueError) as error:
        print(error)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
