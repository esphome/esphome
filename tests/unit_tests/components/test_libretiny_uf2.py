"""Tests for trimming a LibreTiny UF2 to the images a device writes over OTA."""

from pathlib import Path
import struct

from esphome.components.libretiny.uf2 import (
    UF2_BLOCK_SIZE,
    UF2_FLAG_HAS_TAGS,
    UF2_MAGIC_1,
    UF2_MAGIC_2,
    UF2_MAGIC_3,
    UF2_TAG_OTA_PART_INFO,
    device_ota_uf2,
    write_device_ota_uf2,
)

FAMILY_FLAG = 0x2000
FAMILY = 0x7B3EF230
TAG_BINPATCH = 0xB948DE
# OTA_PART_INFO index nibbles: device single, dual 1, dual 2, flasher single, dual 1, dual 2
DEVICE_SINGLE = b"\x10\x00\x00"
FLASHER_SINGLE = b"\x00\x01\x00"
DEVICE_DUAL_1 = b"\x01\x00\x00"
DEVICE_DUAL_2 = b"\x00\x10\x00"


def _tag(tag: int, value: bytes) -> bytes:
    size = 4 + len(value)
    return bytes([size]) + tag.to_bytes(3, "little") + value + bytes(-size % 4)


def _block(payload: bytes = b"", tags: dict[int, bytes] | None = None) -> bytes:
    flags = FAMILY_FLAG | (UF2_FLAG_HAS_TAGS if tags else 0)
    header = struct.pack(
        "<8I", UF2_MAGIC_1, UF2_MAGIC_2, flags, 0, len(payload), 0, 0, FAMILY
    )
    body = payload + b"".join(_tag(t, v) for t, v in (tags or {}).items())
    block = header + body + bytes(UF2_BLOCK_SIZE - 36 - len(body))
    return block + UF2_MAGIC_3.to_bytes(4, "little")


def _image(part_info: bytes, blocks: int, fill: int) -> list[bytes]:
    """An image: its first block names the partitions, the rest carry data."""
    first = _block(
        bytes([fill]) * 256, {UF2_TAG_OTA_PART_INFO: part_info + b"part\x00"}
    )
    return [first] + [_block(bytes([fill]) * 256) for _ in range(blocks - 1)]


def _numbered(blocks: list[bytes]) -> list[bytes]:
    return [
        block[:20] + struct.pack("<2I", seq, len(blocks)) + block[28:]
        for seq, block in enumerate(blocks)
    ]


HEADER = _block(tags={0x6C8492: b"\x02"})


def test_single_ota_package_keeps_only_the_device_image() -> None:
    flasher = _image(FLASHER_SINGLE, 5, 0xAA)
    device = _image(DEVICE_SINGLE, 3, 0xBB)
    package = b"".join(_numbered([HEADER, *flasher, *device]))

    trimmed = device_ota_uf2(package)

    assert trimmed == b"".join(_numbered([HEADER, *device]))


def test_dual_ota_package_keeps_both_device_slots() -> None:
    slot1 = _image(DEVICE_DUAL_1, 2, 0x11)
    flasher = _image(FLASHER_SINGLE, 4, 0xAA)
    slot2 = _image(DEVICE_DUAL_2, 2, 0x22)
    package = b"".join(_numbered([HEADER, *slot1, *flasher, *slot2]))

    trimmed = device_ota_uf2(package)

    assert trimmed == b"".join(_numbered([HEADER, *slot1, *slot2]))


def test_dropped_image_block_with_a_tag_the_device_acts_on_is_kept() -> None:
    flasher = _image(FLASHER_SINGLE, 3, 0xAA)
    patched = _block(bytes([0xAA]) * 256, {TAG_BINPATCH: b"\x01\x02\x03\x04"})
    device = _image(DEVICE_SINGLE, 2, 0xBB)
    package = b"".join(_numbered([HEADER, *flasher, patched, *device]))

    trimmed = device_ota_uf2(package)

    assert trimmed == b"".join(_numbered([HEADER, patched, *device]))


def test_package_with_nothing_to_drop_is_returned_as_is() -> None:
    package = b"".join(_numbered([HEADER, *_image(DEVICE_SINGLE, 3, 0xBB)]))

    assert device_ota_uf2(package) is package


def test_data_that_is_not_a_uf2_is_returned_as_is() -> None:
    package = b"".join(
        _numbered(
            [HEADER, *_image(FLASHER_SINGLE, 2, 0xAA), *_image(DEVICE_SINGLE, 2, 0xBB)]
        )
    )
    not_uf2 = bytes(range(256)) * 4
    bad_magic = package[:UF2_BLOCK_SIZE] + b"\x00" + package[UF2_BLOCK_SIZE + 1 :]

    for data in (b"", not_uf2, package[:-1], bad_magic):
        assert device_ota_uf2(data) is data


def test_write_device_ota_uf2(tmp_path: Path) -> None:
    flasher = _image(FLASHER_SINGLE, 2, 0xAA)
    device = _image(DEVICE_SINGLE, 2, 0xBB)
    firmware = tmp_path / "firmware.uf2"
    firmware.write_bytes(b"".join(_numbered([HEADER, *flasher, *device])))
    out = tmp_path / "build" / "firmware.ota.uf2"

    assert write_device_ota_uf2(firmware, out) == out
    assert out.read_bytes() == b"".join(_numbered([HEADER, *device]))

    # Nothing to drop: the original file is sent and nothing is written
    out.unlink()
    firmware.write_bytes(out_bytes := b"".join(_numbered([HEADER, *device])))
    assert write_device_ota_uf2(firmware, out) == firmware
    assert not out.exists()
    assert firmware.read_bytes() == out_bytes
