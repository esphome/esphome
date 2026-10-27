"""Tell the target platform of a firmware image from its header.

An upload without a configuration has no ``esp32:`` or ``esp8266:`` block
to pick the esphome OTA port from, so it reads the image instead. The
esphome OTA platform takes its default ports from here, so both stay in
sync. Lives outside the component so the upload path never loads it.
"""

from __future__ import annotations

import struct

from esphome.const import (
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_HOST,
    PLATFORM_LN882X,
    PLATFORM_RP2,
    PLATFORM_RTL87XX,
)

DEFAULT_OTA_PORTS: dict[str, int] = {
    PLATFORM_ESP8266: 8266,
    PLATFORM_ESP32: 3232,
    PLATFORM_RP2: 2040,
    PLATFORM_BK72XX: 8892,
    PLATFORM_LN882X: 8820,
    PLATFORM_RTL87XX: 8892,
    PLATFORM_HOST: 8082,
}

_ESP_IMAGE_MAGIC = 0xE9
_ESP_PARTITION_TABLE_MAGIC = b"\xaa\x50"
# The first segment of an ESP8266 image loads into IRAM or DRAM. At the same
# offset an ESP32 image has its write protect pin and SPI drive settings.
_ESP8266_LOAD_RANGE = range(0x3FFE8000, 0x40110000)

_UF2_MAGIC_HEAD = struct.pack("<2I", 0x0A324655, 0x9E5D5157)
_UF2_FLAG_FAMILY_ID = 0x2000
# LibreTiny family IDs (families.json in libretiny-eu/libretiny)
_UF2_FAMILIES = {
    0xAFE81D49: PLATFORM_BK72XX,  # BK7231Q
    0x675A40B0: PLATFORM_BK72XX,  # BK7231T
    0x7B3EF230: PLATFORM_BK72XX,  # BK7231N
    0x159AC324: PLATFORM_BK72XX,  # BK7238
    0x6A82CC42: PLATFORM_BK72XX,  # BK7251
    0xA38090A8: PLATFORM_LN882X,  # LN882H
    0x22E0D6FC: PLATFORM_RTL87XX,  # RTL8710B
    0xE08F7564: PLATFORM_RTL87XX,  # RTL8720C
}

# RP2040 flash starts with a 252 byte second stage bootloader and its CRC
_RP2040_BOOT2_SIZE = 252
# RP2350 boots from an IMAGE_DEF block in the first 4 KB of flash
_RP2350_BLOCK_START = struct.pack("<I", 0xFFFFDED3)
_RP2350_BLOCK_END = struct.pack("<I", 0xAB123579)
_RP2350_BLOCK_SEARCH = 4096


def _crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            crc = (crc << 1) ^ 0x04C11DB7 if crc & 0x80000000 else crc << 1
        crc &= 0xFFFFFFFF
    return crc


def _has_rp2350_block(image: bytes) -> bool:
    for start in range(0, min(len(image), _RP2350_BLOCK_SEARCH) - 4, 4):
        if image[start : start + 4] == _RP2350_BLOCK_START:
            return image.find(_RP2350_BLOCK_END, start, start + 256) != -1
    return False


def detect_platform(image: bytes) -> str | None:
    """The platform an app, bootloader or partition table image was built
    for, or None when the header does not say."""
    if len(image) >= 16 and image[0] == _ESP_IMAGE_MAGIC:
        (load_address,) = struct.unpack_from("<I", image, 8)
        if load_address in _ESP8266_LOAD_RANGE:
            return PLATFORM_ESP8266
        return PLATFORM_ESP32
    if image[:2] == _ESP_PARTITION_TABLE_MAGIC:
        return PLATFORM_ESP32
    if len(image) >= 32 and image[:8] == _UF2_MAGIC_HEAD:
        (flags,) = struct.unpack_from("<I", image, 8)
        if not flags & _UF2_FLAG_FAMILY_ID:
            return None
        return _UF2_FAMILIES.get(struct.unpack_from("<I", image, 28)[0])
    if (
        len(image) >= _RP2040_BOOT2_SIZE + 4
        and _crc32_mpeg2(image[:_RP2040_BOOT2_SIZE])
        == struct.unpack_from("<I", image, _RP2040_BOOT2_SIZE)[0]
    ):
        return PLATFORM_RP2
    if _has_rp2350_block(image):
        return PLATFORM_RP2
    return None
