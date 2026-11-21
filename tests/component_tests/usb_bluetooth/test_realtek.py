"""Tests for the Realtek firmware parser and the usb_bluetooth code generation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import struct

import pytest

from esphome.components.usb_bluetooth import realtek

PROJECT_8761 = 14  # project id of the RTL8761 family -> lmp_subver 0x8761


def _tail(project: int = PROJECT_8761) -> bytes:
    # Instructions are read backwards from before the extension signature: (data, length, opcode)
    return bytes((project, 1, 0)) + realtek.EXTENSION_SIG


def make_v1(patches: list[tuple[int, bytes]], fw_version: int = 0x12345678) -> bytes:
    """An epatch v1 file with (chip id, patch) entries."""
    count = len(patches)
    table_end = realtek.EPATCH_HEADER_LEN + 8 * count
    offsets = []
    body = b""
    for _, data in patches:
        offsets.append(table_end + len(body))
        body += data
    out = realtek.EPATCH_SIGNATURE + struct.pack("<IH", fw_version, count)
    out += struct.pack(f"<{count}H", *(c for c, _ in patches))
    out += struct.pack(f"<{count}H", *(len(d) for _, d in patches))
    out += struct.pack(f"<{count}I", *offsets)
    return out + body + _tail()


def make_v2(sections: list[tuple[int, list[tuple[int, int, bytes]]]]) -> bytes:
    """An epatch v2 file with (opcode, [(eco, prio, data)]) sections."""
    out = realtek.EPATCH_SIGNATURE_V2 + bytes(8) + struct.pack("<I", len(sections))
    for opcode, subs in sections:
        section = struct.pack("<HH", len(subs), 0)
        for eco, prio, data in subs:
            section += struct.pack("<BBHI", eco, prio, 0, len(data)) + data
        out += struct.pack("<II", opcode, len(section)) + section
    return out + _tail()


def make_config(payload: bytes = b"\x01\x02\x03") -> bytes:
    return struct.pack("<IH", realtek.CONFIG_MAGIC, len(payload)) + payload


# --- epatch v1 ---


def test_v1_patch_per_rom_version() -> None:
    images, lmp = realtek.images(make_v1([(1, b"AAAA0000"), (2, b"BBBB1111")]), b"")
    assert lmp == 0x8761
    # Chip id N is for ROM version N - 1, and the last four bytes become the firmware version
    assert images == {
        0: b"AAAA" + struct.pack("<I", 0x12345678),
        1: b"BBBB" + struct.pack("<I", 0x12345678),
    }


def test_v1_config_is_appended() -> None:
    config = make_config()
    images, _ = realtek.images(make_v1([(1, b"AAAA0000")]), config)
    assert images[0].endswith(config)


@pytest.mark.parametrize("chip_id", [0, 257])
def test_v1_rejects_chip_id_out_of_range(chip_id: int) -> None:
    with pytest.raises(realtek.NotFirmware, match="chip id"):
        realtek.images(make_v1([(chip_id, b"AAAA0000")]), b"")


def test_v1_first_patch_for_a_chip_id_wins() -> None:
    images, _ = realtek.images(make_v1([(1, b"AAAA0000"), (1, b"BBBB0000")]), b"")
    assert images[0].startswith(b"AAAA")


def test_v1_rejects_patch_past_the_end() -> None:
    fw = bytearray(make_v1([(1, b"AAAA0000")]))
    # Length of the first patch, after the chip id table
    struct.pack_into("<H", fw, realtek.EPATCH_HEADER_LEN + 2, 0x7FFF)
    with pytest.raises(realtek.NotFirmware, match="does not fit"):
        realtek.images(bytes(fw), b"")


# --- epatch v2 ---


def test_v2_subsections_in_prio_order() -> None:
    fw = make_v2(
        [
            (realtek.PATCH_SNIPPETS, [(2, 5, b"late"), (2, 1, b"early")]),
            (realtek.PATCH_DUMMY_HEADER, [(3, 0, b"other")]),
        ]
    )
    images, lmp = realtek.images(fw, b"")
    assert lmp == 0x8761
    assert images == {1: b"earlylate", 2: b"other"}


def test_v2_rejects_truncated_subsection() -> None:
    fw = bytearray(make_v2([(realtek.PATCH_SNIPPETS, [(2, 0, b"data")])]))
    # The subsection length field: header (20), section header (8), subsection count (4), eco/prio/cb (4)
    struct.pack_into("<I", fw, 20 + 8 + 4 + 4, 1000)
    with pytest.raises(realtek.NotFirmware, match="subsection"):
        realtek.images(bytes(fw), b"")


def test_v2_rejects_truncated_section() -> None:
    fw = bytearray(make_v2([(realtek.PATCH_SNIPPETS, [(2, 0, b"data")])]))
    struct.pack_into("<I", fw, 20 + 4, 1000)  # the section length
    with pytest.raises(realtek.NotFirmware, match="section runs past"):
        realtek.images(bytes(fw), b"")


def test_v2_rejects_security_headers() -> None:
    fw = make_v2([(realtek.PATCH_SECURITY_HEADER, [(1, 0, b"key")])])
    with pytest.raises(realtek.NotFirmware, match="security"):
        realtek.images(fw, b"")


# --- general checks ---


@pytest.mark.parametrize(
    ("data", "match"),
    [
        (b"short", "too short"),
        (b"NotAFirmwareFile", "signature"),
    ],
)
def test_rejects_non_firmware(data: bytes, match: str) -> None:
    with pytest.raises(realtek.NotFirmware, match=match):
        realtek.images(data, b"")


def test_rejects_truncated_file() -> None:
    with pytest.raises(realtek.NotFirmware, match="extension signature"):
        realtek.images(make_v1([(1, b"AAAA0000")])[:-1], b"")


def test_rejects_unknown_project() -> None:
    fw = make_v1([(1, b"AAAA0000")])
    fw = fw[: -len(realtek.EXTENSION_SIG) - 3] + _tail(project=99)
    with pytest.raises(realtek.NotFirmware, match="unknown project id 99"):
        realtek.images(fw, b"")


def test_rejects_bad_config() -> None:
    with pytest.raises(realtek.NotFirmware, match="config"):
        realtek.images(make_v1([(1, b"AAAA0000")]), b"\x00" * 8)


# --- code generation ---


def _write_config(tmp_path: Path, entries: str) -> Path:
    (tmp_path / "fw.bin").write_bytes(make_v1([(1, b"AAAA0000"), (2, b"BBBB1111")]))
    (tmp_path / "fw_config.bin").write_bytes(make_config())
    path = tmp_path / "test.yaml"
    path.write_text(
        "esphome:\n  name: test\n\n"
        "esp32:\n  variant: esp32p4\n  framework:\n    type: esp-idf\n\n"
        "usb_host:\n  max_packet_size: 512\n\n"
        "usb_bluetooth:\n" + entries
    )
    return path


def test_codegen_stores_each_patch_once(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    path = _write_config(
        tmp_path,
        "  - id: bt_a\n    vid: 0x2357\n    pid: 0x0604\n"
        "    firmware: fw.bin\n    firmware_config: fw_config.bin\n"
        "  - id: bt_b\n    vid: 0x0BDA\n    pid: 0x8771\n"
        "    firmware: fw.bin\n    firmware_config: fw_config.bin\n",
    )
    main = generate_main(path)
    assert main.count("bt_a->set_realtek_firmware_storage(") == 1
    assert main.count("bt_a->add_realtek_firmware(") == 2
    assert main.count("bt_b->add_realtek_firmware(") == 2
    # Both entries share the two arrays
    assert main.count("static constexpr uint8_t") == 2


def test_codegen_without_firmware(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    main = generate_main(_write_config(tmp_path, "  - id: bt_a\n"))
    assert "add_realtek_firmware" not in main
    assert "set_realtek_firmware_storage" not in main
