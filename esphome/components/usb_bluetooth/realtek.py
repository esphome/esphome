"""Realtek Bluetooth firmware (linux-firmware rtl_bt/*_fw.bin, epatch v1 and v2), prepared at build time.

A Realtek controller runs its ROM until a patch is downloaded into it. This turns the firmware
file (and its optional config file) into the bytes each ROM version is sent, following
rtlbt_parse_firmware(), rtlbt_parse_firmware_v2() and btrtl_setup_rtl8723b() in Linux's
drivers/bluetooth/btrtl.c; the device only cuts them into download fragments.
"""

# Taken from p1ngb4ck/esphome-usb-drivers (usb_bluetooth/realtek.py, MIT License, (c) 2026 p1ngb4ck).

from __future__ import annotations

import struct

EPATCH_SIGNATURE = b"Realtech"
EPATCH_SIGNATURE_V2 = b"RTBTCore"
EXTENSION_SIG = bytes((0x51, 0x04, 0xFD, 0x77))
EPATCH_HEADER_LEN = 14  # signature, fw_version (u32), num_patches (u16)
EPATCH_V2_HEADER_LEN = 20  # signature, fw_version (8 bytes), num_sections (u32)
EPATCH_V2_TAIL_LEN = 7  # not part of the sections
PATCH_SNIPPETS = 0x01
PATCH_DUMMY_HEADER = 0x02
PATCH_SECURITY_HEADER = 0x03
CONFIG_MAGIC = 0x8723AB55

# project id -> lmp_subver of the chip the file is for (project_id_to_lmp_subver in btrtl.c)
PROJECT_ID_LMP = {
    0: 0x1200,
    1: 0x8723,
    2: 0x8821,
    3: 0x8761,
    7: 0x8703,
    8: 0x8822,
    9: 0x8723,
    10: 0x8821,
    13: 0x8822,
    14: 0x8761,
    18: 0x8852,
    20: 0x8852,
    25: 0x8852,
    36: 0x8851,
    44: 0x8922,
    47: 0x8852,
    51: 0x8761,
}


class NotFirmware(Exception):
    """Not a Realtek epatch file this can read."""


def _project_id(fw: bytes) -> int:
    # Instructions are (data, length, opcode) read backwards from before the extension
    # signature; opcode 0 with length 1 carries the project id, 0xFF ends them.
    at = len(fw) - len(EXTENSION_SIG)
    while at >= EPATCH_HEADER_LEN + 3:
        opcode, length, data = fw[at - 1], fw[at - 2], fw[at - 3]
        at -= 3
        if opcode == 0xFF:
            break
        if length == 0:
            raise NotFirmware("an instruction with length 0")
        if opcode == 0 and length == 1:
            return data
        at -= length
    raise NotFirmware("no project id in the file")


def _subsections(section: bytes) -> list[tuple[int, int, bytes]]:
    """(eco, prio, data) of one section; btrtl_parse_section() stops at the first that does not fit."""
    if len(section) < 4:
        raise NotFirmware("section header runs past the end of the file")
    (count,) = struct.unpack_from("<H", section)
    out = []
    at = 4
    for _ in range(count):
        if len(section) - at < 8:
            break
        eco, prio, _cb, length = struct.unpack_from("<BBHI", section, at)
        at += 8
        if len(section) - at < length:
            break
        out.append((eco, prio, section[at : at + length]))
        at += length
    return out


def _images_v2(fw: bytes) -> dict[int, bytes]:
    """{ROM version: patch}: the snippet and dummy subsections for it, in btrtl's prio order."""
    end = len(fw) - EPATCH_V2_TAIL_LEN
    (num_sections,) = struct.unpack_from("<I", fw, 16)
    by_eco: dict[int, list[tuple[int, bytes]]] = {}
    at = EPATCH_V2_HEADER_LEN
    for _ in range(num_sections):
        if end - at < 8:
            break
        opcode, length = struct.unpack_from("<II", fw, at)
        at += 8
        if end - at < length:
            break
        section = fw[at : at + length]
        at += length
        if opcode == PATCH_SECURITY_HEADER:
            # Taken by a controller reporting a non-zero key id, which is not read here.
            if _subsections(section):
                raise NotFirmware(
                    "security headers (epatch v2 with key ids) are not supported"
                )
            continue
        if opcode not in (PATCH_SNIPPETS, PATCH_DUMMY_HEADER):
            continue
        for eco, prio, data in _subsections(section):
            subs = by_eco.setdefault(eco, [])
            # btrtl_insert_ordered_subsec(): before the first one of the same or a higher prio.
            index = next((i for i, (p, _) in enumerate(subs) if p >= prio), len(subs))
            subs.insert(index, (prio, data))
    # A controller reporting ROM version N takes the subsections with eco N + 1.
    out = {
        eco - 1: b"".join(data for _, data in subs)
        for eco, subs in by_eco.items()
        if eco > 0 and any(data for _, data in subs)
    }
    if not out:
        raise NotFirmware("no patches in the file")
    return out


def images(fw: bytes, config: bytes) -> tuple[dict[int, bytes], int]:
    """{ROM version: bytes to download} and the lmp_subver of the chip the file is for."""
    if len(fw) <= 8:
        raise NotFirmware("too short")
    if fw[:8] == EPATCH_SIGNATURE:
        header_len = EPATCH_HEADER_LEN
    elif fw[:8] == EPATCH_SIGNATURE_V2:
        header_len = EPATCH_V2_HEADER_LEN
    else:
        raise NotFirmware("no Realtech or RTBTCore signature")
    if len(fw) < header_len + len(EXTENSION_SIG) + 3:
        raise NotFirmware("too short")
    if fw[-len(EXTENSION_SIG) :] != EXTENSION_SIG:
        raise NotFirmware("extension signature missing (truncated file?)")

    project = _project_id(fw)
    if project not in PROJECT_ID_LMP:
        raise NotFirmware(f"unknown project id {project}")

    if config:
        if len(config) < 6:
            raise NotFirmware("config file too short")
        magic, total = struct.unpack_from("<IH", config)
        if magic != CONFIG_MAGIC or total + 6 != len(config):
            raise NotFirmware("not a Realtek config file")

    if header_len == EPATCH_V2_HEADER_LEN:
        return {
            rom: patch + config for rom, patch in _images_v2(fw).items()
        }, PROJECT_ID_LMP[project]

    fw_version, count = struct.unpack_from("<IH", fw, 8)
    if count == 0:
        raise NotFirmware("no patches in the file")
    if len(fw) < EPATCH_HEADER_LEN + len(EXTENSION_SIG) + 3 + 8 * count:
        raise NotFirmware("patch table runs past the end of the file")
    base = EPATCH_HEADER_LEN
    chip_ids = struct.unpack_from(f"<{count}H", fw, base)
    lengths = struct.unpack_from(f"<{count}H", fw, base + 2 * count)
    offsets = struct.unpack_from(f"<{count}I", fw, base + 4 * count)

    out: dict[int, bytes] = {}
    for chip_id, length, offset in zip(chip_ids, lengths, offsets, strict=True):
        if length < 4 or offset > len(fw) or length > len(fw) - offset:
            raise NotFirmware(f"patch for chip id {chip_id} does not fit in the file")
        image = bytearray(fw[offset : offset + length])
        # The last four bytes become the header's firmware version.
        image[length - 4 : length] = struct.pack("<I", fw_version)
        # A controller reporting ROM version N takes the patch with chip id N + 1.
        out[chip_id - 1] = bytes(image) + config
    return out, PROJECT_ID_LMP[project]
