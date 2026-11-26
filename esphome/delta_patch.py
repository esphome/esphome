"""Build an esp_delta_ota patch for `esphome upload --delta-from`.

The format is Espressif's esp_delta_ota one (esp_delta_ota_patch_gen.py): a 64-byte header, then a detools patch
with heatshrink compression::

    0   4   magic 0xfccdde10, little endian
    4   32  SHA-256 of the base app image (the device compares it with its running app)
    36  16  MD5 of the new app image (ESPHome's addition, in the reserved bytes: the upload's checksum)
    52  12  reserved, zero

The device rebuilds the new image from its running app and the patch, writes it to the update slot, and checks the
MD5 of what it wrote before it switches to it.
"""

from __future__ import annotations

import hashlib
import io
from pathlib import Path

from esphome.core import EsphomeError

DELTA_MAGIC = 0xFCCDDE10
HEADER_SIZE = 64
BASE_SHA_OFFSET = 4
NEW_MD5_OFFSET = 36


def _image_sha256(image: bytes) -> bytes:
    """The SHA-256 an ESP32 app image carries after its segments (esp_partition_get_sha256 returns it).

    Found by walking the segments rather than taken from the end of the file: a signed image has padding and a
    signature block after the digest.
    """
    if len(image) < 24 or image[0] != 0xE9 or image[23] != 1:
        raise EsphomeError(
            "The base is not an ESP32 app image with an appended SHA-256"
        )
    offset = 24
    for _ in range(image[1]):
        if offset + 8 > len(image):
            raise EsphomeError("The base image is truncated")
        offset += 8 + int.from_bytes(image[offset + 4 : offset + 8], "little")
    offset += 16 - offset % 16
    digest = image[offset : offset + 32]
    if len(digest) != 32 or hashlib.sha256(image[:offset]).digest() != digest:
        raise EsphomeError(
            "The base is not an ESP32 app image with an appended SHA-256"
        )
    return digest


def make_patch(base: Path, new: Path) -> bytes:
    try:
        import detools
    except ImportError as err:
        raise EsphomeError(
            "--delta-from needs the 'detools' package: pip install detools"
        ) from err
    try:
        base_img = base.read_bytes()
        new_img = new.read_bytes()
    except OSError as err:
        raise EsphomeError(f"Cannot read {err.filename}: {err.strerror}") from err
    base_sha = _image_sha256(base_img)
    out = io.BytesIO()
    detools.create_patch(
        io.BytesIO(base_img), io.BytesIO(new_img), out, compression="heatshrink"
    )
    body = out.getvalue()
    rebuilt = io.BytesIO()
    detools.apply_patch(io.BytesIO(base_img), io.BytesIO(body), rebuilt)
    if rebuilt.getvalue() != new_img:
        raise EsphomeError("The delta patch does not rebuild the new image")
    header = bytearray(HEADER_SIZE)
    header[0:4] = DELTA_MAGIC.to_bytes(4, "little")
    header[BASE_SHA_OFFSET : BASE_SHA_OFFSET + 32] = base_sha
    header[NEW_MD5_OFFSET : NEW_MD5_OFFSET + 16] = hashlib.md5(new_img).digest()
    return bytes(header) + body


def new_image_md5(patch: bytes) -> str:
    """The MD5 the device must end up with, as hex (what the upload sends as its checksum)."""
    if len(patch) < HEADER_SIZE or int.from_bytes(patch[0:4], "little") != DELTA_MAGIC:
        raise EsphomeError("Not an ESPHome delta patch")
    return patch[NEW_MD5_OFFSET : NEW_MD5_OFFSET + 16].hex()


def main(argv: list[str] | None = None) -> int:
    """Python -m esphome.delta_patch BASE NEW OUT: write the patch, print the base SHA-256 and the patch MD5.

    For publishing a patch beside a full image in an http_request update manifest ("delta" in the ota object).
    """
    import sys

    args = sys.argv[1:] if argv is None else argv
    if len(args) != 3:
        print("usage: python -m esphome.delta_patch BASE NEW OUT", file=sys.stderr)
        return 2
    base, new, out = (Path(a) for a in args)
    try:
        patch = make_patch(base, new)
        out.write_bytes(patch)
    except (EsphomeError, OSError) as err:
        print(f"error: {err}", file=sys.stderr)
        return 1
    print(
        _image_sha256(base.read_bytes()).hex(),
        hashlib.md5(patch).hexdigest(),
        len(patch),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
