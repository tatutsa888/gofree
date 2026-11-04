#!/usr/bin/env python3
"""Build and check GENIbus telegrams for the Grundfos SCALA1 (GoFree).

    geni.py read 91 164      -> status request
    geni.py cmd 5            -> stop command
    geni.py check 2707e7f80a035b00a412a3
"""
import sys


def crc(data: bytes) -> int:
    c = 0xFFFF
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else (c << 1)
            c &= 0xFFFF
    return (~c) & 0xFFFF


def frame(apdu: bytes, start=0x27, dst=0xE7, src=0xF8) -> bytes:
    body = bytes([len(apdu) + 2, dst, src]) + apdu
    c = crc(body)
    return bytes([start]) + body + bytes([c >> 8, c & 0xFF])


def main(argv):
    if len(argv) >= 3 and argv[0] == "read":
        i, sub = int(argv[1]), int(argv[2])
        print(frame(bytes([0x0A, 0x03, i, sub >> 8, sub & 0xFF])).hex(" "))
    elif len(argv) >= 2 and argv[0] == "cmd":
        print(frame(bytes([0x03, 0x81, int(argv[1])])).hex(" "))
    elif len(argv) >= 2 and argv[0] == "check":
        b = bytes.fromhex(argv[1].replace(" ", "").replace(".", ""))
        ok = crc(b[1:-2]) == int.from_bytes(b[-2:], "big")
        print("CRC ok" if ok else "CRC wrong")
        return 0 if ok else 1
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
