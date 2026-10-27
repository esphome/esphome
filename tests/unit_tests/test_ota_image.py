"""Tests for telling the platform of a firmware image from its header."""

from __future__ import annotations

import struct

import pytest

from esphome.const import (
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)
from esphome.ota_image import _UF2_MAGIC_HEAD, _crc32_mpeg2, detect_platform


def _uf2_block(family: int, flags: int = 0x2000) -> bytes:
    header = _UF2_MAGIC_HEAD + struct.pack("<6I", flags, 0, 256, 0, 1, family)
    return header + bytes(512 - len(header))


def _rp2040_image() -> bytes:
    boot2 = bytes(range(252))
    return boot2 + struct.pack("<I", _crc32_mpeg2(boot2)) + bytes(256)


def _rp2350_image() -> bytes:
    # A minimal IMAGE_DEF block at 0x138, where the RP2350 blink example has it
    block = bytes.fromhex("d3deffff42012110ff01000070f70300793512ab")
    return bytes(0x138) + block + bytes(256)


def _rp2350_image_block_in_last_word() -> bytes:
    # The block starts in the last word of the 4 KB the boot ROM searches
    block = bytes.fromhex("d3deffff42012110ff01000070f70300793512ab")
    return bytes(4092) + block + bytes(256)


def test_crc32_mpeg2_check_value() -> None:
    """The standard check value of CRC-32/MPEG-2, the RP2040 boot2 checksum."""
    assert _crc32_mpeg2(b"123456789") == 0x0376E6E7


@pytest.mark.parametrize(
    ("image", "platform"),
    [
        # Real ESP32-S3, ESP32 and ESP8266 app image headers
        (bytes.fromhex("e905022f645f3740ee00000009000000"), PLATFORM_ESP32),
        (bytes.fromhex("e9060220fc200840ee00000000000000"), PLATFORM_ESP32),
        (bytes.fromhex("e902034080f4104000f01040600d0000"), PLATFORM_ESP8266),
        (b"\xaa\x50" + bytes(30), PLATFORM_ESP32),
        (_uf2_block(0x6A82CC42), PLATFORM_BK72XX),
        (_uf2_block(0x7B3EF230), PLATFORM_BK72XX),
        (_uf2_block(0x22E0D6FC), PLATFORM_RTL87XX),
        (_uf2_block(0xE08F7564), PLATFORM_RTL87XX),
        (_uf2_block(0xA38090A8), PLATFORM_LN882X),
        (_rp2040_image(), PLATFORM_RP2),
        (_rp2350_image(), PLATFORM_RP2),
        (_rp2350_image_block_in_last_word(), PLATFORM_RP2),
    ],
    ids=[
        "esp32-s3",
        "esp32",
        "esp8266",
        "esp32-partition-table",
        "bk7251",
        "bk7231n",
        "rtl8710b",
        "rtl8720c",
        "ln882h",
        "rp2040",
        "rp2350",
        "rp2350-block-in-last-word",
    ],
)
def test_detect_platform(image: bytes, platform: str) -> None:
    assert detect_platform(image) == platform


@pytest.mark.parametrize(
    "image",
    [
        b"",
        bytes(512),
        b"\xff" * 4096,
        b"\x1f\x8b" + bytes(30),
        _uf2_block(0xE48BFF56),
        _uf2_block(0x6A82CC42, flags=0),
        bytes(0x138) + bytes.fromhex("d3deffff") + bytes(512),
    ],
    ids=[
        "empty",
        "zeros",
        "erased-flash",
        "gzip",
        "uf2-other-family",
        "uf2-without-family",
        "rp2350-block-without-end",
    ],
)
def test_detect_platform_unknown(image: bytes) -> None:
    assert detect_platform(image) is None
