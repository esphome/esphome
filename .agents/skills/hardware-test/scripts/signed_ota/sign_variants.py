"""Sign an ESP-IDF app image and write the signed OTA cases: key A, key B, B then A, and CRC-only edits."""

import argparse
import pathlib
import struct
import subprocess
import sys
import zlib

BLOCK = 1216
CRC_AT = 1196


def sig_sector(data: bytes) -> int:
    # The image can hold E7 02 at a 4 KiB boundary by chance; the real block has a matching CRC
    for off in range(0, len(data) - BLOCK, 4096):
        b = data[off : off + BLOCK]
        if (
            b[0] == 0xE7
            and b[1] == 0x02
            and struct.unpack("<I", b[CRC_AT : CRC_AT + 4])[0] == zlib.crc32(b[:CRC_AT])
        ):
            return off
    raise SystemExit("no signature block")


def sign(image: str, out: str, *keys: str) -> bytes:
    args = [sys.executable, "-m", "espsecure", "sign-data", "-v", "2"]
    for k in keys:
        args += ["-k", k]
    result = subprocess.run(
        [*args, "-o", out, image], check=False, capture_output=True, text=True
    )
    if result.returncode:
        raise SystemExit(f"espsecure failed for {out}:\n{result.stderr}")
    return pathlib.Path(out).read_bytes()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("image", help="firmware.ota.bin")
    ap.add_argument("key_a", help="trusted key (in verification_keys)")
    ap.add_argument("key_b", help="untrusted key")
    ap.add_argument("prefix")
    a = ap.parse_args()
    signed_a = sign(a.image, f"{a.prefix}.A.bin", a.key_a)
    sign(a.image, f"{a.prefix}.B.bin", a.key_b)
    signed_ba = sign(a.image, f"{a.prefix}.BA.bin", a.key_b, a.key_a)
    o = sig_sector(signed_a)
    pad = bytearray(signed_a)
    pad[o + 2] ^= 0x5A  # covered only by the CRC
    pathlib.Path(f"{a.prefix}.A.pad.bin").write_bytes(pad)
    pad[o + CRC_AT : o + CRC_AT + 4] = struct.pack(
        "<I", zlib.crc32(bytes(pad[o : o + CRC_AT]))
    )
    pathlib.Path(f"{a.prefix}.A.padfix.bin").write_bytes(pad)
    crc = bytearray(signed_a)
    crc[o + CRC_AT] ^= 0x01
    pathlib.Path(f"{a.prefix}.A.crc.bin").write_bytes(crc)
    o2 = sig_sector(signed_ba) + BLOCK
    ba = bytearray(signed_ba)
    ba[o2 + CRC_AT] ^= 0x01
    pathlib.Path(f"{a.prefix}.BAcrc.bin").write_bytes(ba)
    print(f"signature sector at {o:#x}")


if __name__ == "__main__":
    main()
