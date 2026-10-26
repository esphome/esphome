"""Trim a LibreTiny UF2 package to the images a device writes over OTA.

A LibreTiny UF2 holds one image per OTA scheme: the device schemes (single,
dual slot 1, dual slot 2) and the flasher schemes used over UART. The device
skips every block of an image whose scheme it does not run (uf2ota), so an
image no device scheme writes is only sent to be thrown away. On single OTA
chips (BK72xx, LN882H) that is the whole flasher image, more than half the
package.
"""

from pathlib import Path
import struct

UF2_BLOCK_SIZE = 512
UF2_MAGIC_1 = 0x0A324655
UF2_MAGIC_2 = 0x9E5D5157
UF2_MAGIC_3 = 0x0AB16F30
UF2_FLAG_HAS_TAGS = 0x8000
# Header words: magic1, magic2, flags, addr, len, block_seq, block_count, family
_HEADER = struct.Struct("<8I")
_PAYLOAD_START = _HEADER.size
_SEQ_OFFSET = 20

# One partition index nibble per scheme, high nibble first: the three device
# schemes are the first three nibbles, the flasher schemes the next three
UF2_TAG_OTA_PART_INFO = 0xC0EE0C
_DEVICE_SCHEME_NIBBLES = 3
# Tags uf2ota acts on beyond the partition of the image they sit in; a block
# that carries one is never dropped
_KEEP_TAGS = frozenset(
    {
        0x5D57D0,  # OTA_FORMAT_1
        0x6C8492,  # OTA_FORMAT_2
        0x6EC68A,  # OTA_PART_LIST
        0xB948DE,  # BINPATCH
        0x8288ED,  # FAL_PTABLE
    }
)


def _tags(block: bytes, length: int) -> dict[int, bytes]:
    """The extension tags after a block's payload."""
    tags: dict[int, bytes] = {}
    pos = _PAYLOAD_START + length
    while pos + 4 <= UF2_BLOCK_SIZE - 4:
        size = block[pos]
        tag = int.from_bytes(block[pos + 1 : pos + 4], "little")
        if size < 4 or tag == 0:
            break
        tags[tag] = block[pos + 4 : pos + size]
        pos += (size + 3) & ~3
    return tags


def _for_a_device(part_info: bytes) -> bool:
    """Whether any device scheme writes the image this OTA_PART_INFO starts."""
    nibbles = [n for b in part_info[:2] for n in (b >> 4, b & 0xF)]
    return any(nibbles[:_DEVICE_SCHEME_NIBBLES])


def device_ota_uf2(data: bytes) -> bytes:
    """Return ``data`` without the images no device scheme writes.

    Anything that is not a well formed UF2 with OTA partition info, or has
    nothing to drop, comes back unchanged.
    """
    if not data or len(data) % UF2_BLOCK_SIZE:
        return data
    kept: list[bytearray] = []
    keep = True
    for at in range(0, len(data), UF2_BLOCK_SIZE):
        block = data[at : at + UF2_BLOCK_SIZE]
        magic1, magic2, flags, _, length, _, _, _ = _HEADER.unpack_from(block)
        magic3 = int.from_bytes(block[-4:], "little")
        if (magic1, magic2, magic3) != (UF2_MAGIC_1, UF2_MAGIC_2, UF2_MAGIC_3):
            return data
        if length > UF2_BLOCK_SIZE - _PAYLOAD_START - 4:
            return data
        tags = _tags(block, length) if flags & UF2_FLAG_HAS_TAGS else {}
        if (part_info := tags.get(UF2_TAG_OTA_PART_INFO)) is not None:
            keep = _for_a_device(part_info)
        # The first block is the package header
        if keep or at == 0 or not _KEEP_TAGS.isdisjoint(tags):
            kept.append(bytearray(block))
    if len(kept) * UF2_BLOCK_SIZE == len(data):
        return data
    # The device takes the update size from the block count
    for seq, block in enumerate(kept):
        struct.pack_into("<2I", block, _SEQ_OFFSET, seq, len(kept))
    return b"".join(kept)


def write_device_ota_uf2(firmware: Path, out: Path) -> Path:
    """Write the device images of ``firmware`` to ``out``; return the file to upload."""
    data = firmware.read_bytes()
    trimmed = device_ota_uf2(data)
    if trimmed is data:
        return firmware
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(trimmed)
    return out
