"""Trim a LibreTiny UF2 package to the images a device writes over OTA.

A LibreTiny UF2 holds one image per OTA scheme: the device schemes (single,
dual slot 1, dual slot 2) and the flasher schemes used over UART. The device
skips every block of an image whose scheme it does not run (uf2ota), so an
image no device scheme writes is only sent to be thrown away. On single OTA
chips (BK72xx, LN882H) that is the whole flasher image, more than half the
package. The legacy OTA format names the same device partitions, so devices
that read it write the same images.
"""

import struct

UF2_BLOCK_SIZE = 512
UF2_MAGIC_HEAD = struct.pack("<2I", 0x0A324655, 0x9E5D5157)
UF2_MAGIC_TAIL = struct.pack("<I", 0x0AB16F30)
UF2_FLAG_HAS_TAGS = 0x8000
# Header words after the magic: flags, target address, payload length, then
# the block number and block count
_FLAGS_ADDR_LEN = struct.Struct("<3I")
_SEQ_OFFSET = 20
_DATA_START = 32
_DATA_END = UF2_BLOCK_SIZE - len(UF2_MAGIC_TAIL)

# One partition index nibble per scheme, high nibble first: device single,
# dual 1 and dual 2 are the first three, the flasher schemes the next three
UF2_TAG_OTA_PART_INFO = 0xC0EE0C
# Tags uf2ota acts on beyond the image they sit in; a block carrying one is
# always kept
_KEEP_TAGS = frozenset(
    {
        0x5D57D0,  # OTA_FORMAT_1
        0x6C8492,  # OTA_FORMAT_2
        0x6EC68A,  # OTA_PART_LIST
        0xB948DE,  # BINPATCH
        0x8288ED,  # FAL_PTABLE
    }
)


def _tags(block: bytes, length: int) -> dict[int, bytes] | None:
    """The extension tags after a block's payload, or None if malformed."""
    tags: dict[int, bytes] = {}
    pos = _DATA_START + length
    while pos + 4 <= _DATA_END and (size := block[pos]):
        if size < 4 or pos + size > _DATA_END:
            return None
        tags[int.from_bytes(block[pos + 1 : pos + 4], "little")] = block[
            pos + 4 : pos + size
        ]
        pos += (size + 3) & ~3
    return tags


def device_ota_uf2(data: bytes) -> bytes | None:
    """``data`` without the images no device scheme writes.

    None when there is nothing to drop, or when ``data`` is not a well
    formed UF2, so the caller sends the file as it is.
    """
    if len(data) % UF2_BLOCK_SIZE:
        return None
    kept: list[bytes] = []
    keep = True
    for at in range(0, len(data), UF2_BLOCK_SIZE):
        block = data[at : at + UF2_BLOCK_SIZE]
        flags, _, length = _FLAGS_ADDR_LEN.unpack_from(block, 8)
        if (
            block[:8] != UF2_MAGIC_HEAD
            or block[_DATA_END:] != UF2_MAGIC_TAIL
            or _DATA_START + length > _DATA_END
        ):
            return None
        tags = _tags(block, length) if flags & UF2_FLAG_HAS_TAGS else {}
        if tags is None:
            return None
        if (part_info := tags.get(UF2_TAG_OTA_PART_INFO)) is not None:
            # Any of the three device scheme nibbles names a partition
            keep = int.from_bytes(part_info[:2], "big") >> 4 != 0
        if keep or not _KEEP_TAGS.isdisjoint(tags):
            kept.append(block)
    if len(kept) * UF2_BLOCK_SIZE == len(data):
        return None
    out = bytearray(b"".join(kept))
    # The device takes the update size from the block count
    for seq in range(len(kept)):
        struct.pack_into("<2I", out, seq * UF2_BLOCK_SIZE + _SEQ_OFFSET, seq, len(kept))
    return bytes(out)
