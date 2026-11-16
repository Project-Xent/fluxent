import math
import os
import struct
import sys
import zlib

OUT = sys.argv[1]


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def png_rows(w, h, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw += bytes(pixel(x, y))
    return zlib.compress(bytes(raw), 9)


def ihdr(w, h):
    return png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))


def write_png(name, w, h, pixel):
    data = b"\x89PNG\r\n\x1a\n" + ihdr(w, h) + png_chunk(b"IDAT", png_rows(w, h, pixel)) + png_chunk(b"IEND", b"")
    open(os.path.join(OUT, name), "wb").write(data)


def write_apng(name, w, h, frames, delay_ms):
    out = bytearray(b"\x89PNG\r\n\x1a\n" + ihdr(w, h))
    out += png_chunk(b"acTL", struct.pack(">II", len(frames), 0))
    seq = 0
    for i, pixel in enumerate(frames):
        out += png_chunk(b"fcTL", struct.pack(">IIIIIHHBB", seq, w, h, 0, 0, delay_ms, 1000, 0, 0))
        seq += 1
        data = png_rows(w, h, pixel)
        if i == 0:
            out += png_chunk(b"IDAT", data)
        else:
            out += png_chunk(b"fdAT", struct.pack(">I", seq) + data)
            seq += 1
    out += png_chunk(b"IEND", b"")
    open(os.path.join(OUT, name), "wb").write(bytes(out))


def lzw(indices, min_size):
    clear, end = 1 << min_size, (1 << min_size) + 1
    out, acc, bits = bytearray(), 0, 0

    def emit(code, size):
        nonlocal acc, bits
        acc |= code << bits
        bits += size
        while bits >= 8:
            out.append(acc & 0xff)
            acc >>= 8
            bits -= 8

    table = {(i,): i for i in range(clear)}
    size, nxt = min_size + 1, end + 1
    emit(clear, size)
    cur = ()
    for px in indices:
        key = cur + (px,)
        if key in table:
            cur = key
            continue
        emit(table[cur], size)
        if nxt < 4096:
            table[key] = nxt
            if nxt == (1 << size) and size < 12:
                size += 1
            nxt += 1
        else:
            emit(clear, size)
            table = {(i,): i for i in range(clear)}
            size, nxt = min_size + 1, end + 1
        cur = (px,)
    if cur:
        emit(table[cur], size)
    emit(end, size)
    if bits:
        out.append(acc & 0xff)
    blocks = bytearray()
    for i in range(0, len(out), 255):
        part = out[i:i + 255]
        blocks += bytes([len(part)]) + part
    return bytes(blocks) + b"\x00"


def write_gif(name, w, h, palette, frames, delay_cs):
    out = bytearray(b"GIF89a" + struct.pack("<HHBBB", w, h, 0xf1, 0, 0))
    for rgb in palette:
        out += bytes(rgb)
    out += b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"
    for indices in frames:
        out += b"\x21\xf9\x04" + struct.pack("<BHBB", 0x09, delay_cs, 0, 0)
        out += b"\x2c" + struct.pack("<HHHHB", 0, 0, w, h, 0)
        out += b"\x02" + lzw(indices, 2)
    out += b"\x3b"
    open(os.path.join(OUT, name), "wb").write(bytes(out))


def spinner_frame(t):
    pixels = []
    for y in range(48):
        for x in range(48):
            dx, dy = x - 23.5, y - 23.5
            r = math.hypot(dx, dy)
            a = (math.atan2(dy, dx) / (2 * math.pi) + 1.0 - t) % 1.0
            on = 15 <= r <= 21
            pixels.append((1 if a < 0.33 else 2) if on else 0)
    return pixels


def ball_frame(t):
    y0 = 8 + 40 * abs(math.sin(math.pi * t))

    def pixel(x, y):
        d = math.hypot(x - 31.5, y - (63 - y0))
        alpha = max(0.0, min(1.0, 9.5 - d))
        return (0, 120, 215, int(alpha * 255)) if alpha else (0, 0, 0, 0)

    return pixel


def frame_pixel(x, y):
    nx, ny = min(x, 47 - x) + 0.5, min(y, 47 - y) + 0.5
    if nx < 12 and ny < 12:
        d = math.hypot(12 - nx, 12 - ny)
        if d > 12:
            return (0, 0, 0, 0)
        edge = d > 10
    else:
        edge = min(nx, ny) < 2
    return (0, 95, 184, 255) if edge else (204, 228, 247, 255)


os.makedirs(OUT, exist_ok=True)
write_gif("spinner.gif", 48, 48, [(0, 0, 0), (0, 120, 215), (153, 201, 239), (255, 255, 255)],
          [spinner_frame(i / 12) for i in range(12)], 8)
write_apng("bounce.png", 64, 64, [ball_frame(i / 16) for i in range(16)], 50)
write_png("frame.png", 48, 48, frame_pixel)
open(os.path.join(OUT, "logo.svg"), "wb").write(
    b'<svg xmlns="http://www.w3.org/2000/svg" width="96" height="96" viewBox="0 0 96 96">'
    b'<rect x="4" y="4" width="88" height="88" rx="16" fill="#0078d4"/>'
    b'<circle cx="48" cy="40" r="18" fill="#ffffff"/>'
    b'<path d="M20 80 Q48 52 76 80 Z" fill="#9ccbf0"/></svg>\n')
