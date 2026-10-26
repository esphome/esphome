"""Tests for trimming a LibreTiny UF2 to the images a device writes over OTA."""

from pathlib import Path
import struct

import pytest

from esphome.components.libretiny import device_ota_image
from esphome.components.libretiny.uf2 import UF2_TAG_OTA_PART_INFO, device_ota_uf2
from esphome.core import CORE

# Written from the UF2 and LibreTiny formats, not from the module under test
MAGIC_1, MAGIC_2, MAGIC_3 = 0x0A324655, 0x9E5D5157, 0x0AB16F30
FLAG_FAMILY, FLAG_TAGS = 0x2000, 0x8000
FAMILY = 0x7B3EF230
TAG_OTA_FORMAT_2 = 0x6C8492
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
    """A block numbered 0 of 0; _package numbers it."""
    flags = FLAG_FAMILY | (FLAG_TAGS if tags else 0)
    header = struct.pack("<8I", MAGIC_1, MAGIC_2, flags, 0, len(payload), 0, 0, FAMILY)
    body = payload + b"".join(_tag(t, v) for t, v in (tags or {}).items())
    return header + body + bytes(512 - 32 - 4 - len(body)) + struct.pack("<I", MAGIC_3)


def _image(part_info: bytes, blocks: int, fill: int) -> list[bytes]:
    """An image: its first block names the partitions, the rest carry data."""
    data = bytes([fill]) * 256
    first = _block(data, {UF2_TAG_OTA_PART_INFO: part_info + b"part\x00"})
    return [first] + [_block(data) for _ in range(blocks - 1)]


def _package(*blocks: bytes) -> bytes:
    """The blocks numbered in order, as a UF2 file."""
    return b"".join(
        block[:20] + struct.pack("<2I", seq, len(blocks)) + block[28:]
        for seq, block in enumerate(blocks)
    )


HEADER = _block(tags={TAG_OTA_FORMAT_2: b"\x02"})
FLASHER = _image(FLASHER_SINGLE, 5, 0xAA)
DEVICE = _image(DEVICE_SINGLE, 3, 0xBB)
SLOT_1 = _image(DEVICE_DUAL_1, 2, 0x11)
SLOT_2 = _image(DEVICE_DUAL_2, 2, 0x22)
PATCHED = _block(bytes([0xAA]) * 256, {TAG_BINPATCH: b"\x01\x02\x03\x04"})


@pytest.mark.parametrize(
    ("blocks", "expected"),
    [
        pytest.param([HEADER, *FLASHER, *DEVICE], [HEADER, *DEVICE], id="single_ota"),
        pytest.param(
            [HEADER, *SLOT_1, *FLASHER, *SLOT_2],
            [HEADER, *SLOT_1, *SLOT_2],
            id="dual_ota_keeps_both_slots",
        ),
        pytest.param(
            [HEADER, *FLASHER, PATCHED, *DEVICE],
            [HEADER, PATCHED, *DEVICE],
            id="keeps_a_tag_the_device_acts_on",
        ),
        pytest.param([HEADER, *DEVICE], None, id="nothing_to_drop"),
    ],
)
def test_device_ota_uf2(blocks: list[bytes], expected: list[bytes] | None) -> None:
    trimmed = device_ota_uf2(_package(*blocks))
    assert trimmed == (_package(*expected) if expected is not None else None)


def test_data_that_is_not_a_well_formed_uf2_is_left_alone() -> None:
    package = _package(HEADER, *FLASHER, *DEVICE)
    bad_magic = package[:512] + b"\x00" + package[513:]
    # A tag that runs past the data area of the first flasher block
    at = 512 + 32 + 256
    bad_tag = package[:at] + bytes([250]) + package[at + 1 :]

    for data in (b"", bytes(range(256)) * 4, package[:-1], bad_magic, bad_tag):
        assert device_ota_uf2(data) is None


def test_device_ota_image(setup_core: Path) -> None:
    tmp_path = setup_core
    CORE.build_path = tmp_path / "build"
    CORE.name = "test"
    firmware = tmp_path / "firmware.uf2"

    firmware.write_bytes(_package(HEADER, *FLASHER, *DEVICE))
    image = device_ota_image(firmware)
    assert image == CORE.relative_pioenvs_path("test", "firmware.ota.uf2")
    assert image.read_bytes() == _package(HEADER, *DEVICE)

    # Nothing to drop: the firmware itself is sent
    firmware.write_bytes(_package(HEADER, *DEVICE))
    assert device_ota_image(firmware) == firmware
