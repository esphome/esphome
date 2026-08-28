"""Tests for esphome.espidf.size_summary.print_summary."""

from __future__ import annotations

import json
from pathlib import Path
import struct

import pytest

from esphome.espidf.size_summary import print_summary


def _write_size_json(tmp_path: Path, data: dict) -> Path:
    """Drop a fake esp_idf_size.json under ``tmp_path`` and return the path."""
    out = tmp_path / "esp_idf_size.json"
    out.write_text(json.dumps(data))
    return out


def _write_partitions(tmp_path: Path) -> Path:
    """Drop a partitions.csv with a 0x1C0000 (1835008 byte) app slot."""
    out = tmp_path / "partitions.csv"
    out.write_text(
        "# name, type, subtype, offset, size, flags\n"
        "app0, app, ota_0, 0x10000, 0x1C0000,\n"
    )
    return out


def _elf_bytes(sections: list[tuple[int, int, int]], shentsize: int = 40) -> bytes:
    """Build a minimal ELF32 LE whose section headers carry the given
    (sh_type, sh_flags, sh_size) triples."""
    out = bytearray(52)
    out[0:4] = b"\x7fELF"
    out[4] = out[5] = 1  # 32-bit, little-endian
    struct.pack_into("<I", out, 0x20, 52)  # e_shoff
    struct.pack_into("<HH", out, 0x2E, shentsize, len(sections))
    for sh_type, sh_flags, sh_size in sections:
        shdr = bytearray(40)
        struct.pack_into("<II", shdr, 4, sh_type, sh_flags)
        struct.pack_into("<I", shdr, 20, sh_size)
        out += shdr
    return bytes(out)


def _esp32_size_data() -> dict:
    """Synthetic json2 for the original ESP32 (split IRAM/DRAM), in the
    esp-idf-size >= 2.1 shape that carries ``total_size``."""
    return {
        "version": "1.1",
        "total_size": 827455,
        "layout": [
            {
                "name": "DRAM",
                "total": 180736,
                "used": 47332,
                "free": 133404,
                "parts": {
                    ".bss": {"size": 30616},
                    ".data": {"size": 16716},
                },
            },
            {
                "name": "IRAM",
                "total": 131072,
                "used": 80351,
                "free": 50721,
                "parts": {
                    ".text": {"size": 79323},
                    ".vectors": {"size": 1028},
                },
            },
        ],
    }


def _s3_size_data() -> dict:
    """Synthetic json2 for ESP32-S3 (unified DIRAM), in the esp-idf-size 1.x
    shape without ``total_size``."""
    return {
        "version": "1.1",
        "layout": [
            {
                "name": "DIRAM",
                "total": 341760,
                "used": 104999,
                "free": 236761,
                "parts": {
                    ".text": {"size": 58051},
                    ".bss": {"size": 27088},
                    ".data": {"size": 19708},
                    ".noinit": {"size": 152},
                },
            },
            {
                "name": "IRAM",
                "total": 16384,
                "used": 16384,
                "free": 0,
                "parts": {
                    ".text": {"size": 15356},
                    ".vectors": {"size": 1028},
                },
            },
        ],
    }


def _print_summary_ram_only(tmp_path: Path, size_json: Path) -> None:
    """Call print_summary with no partitions.csv or ELF on disk."""
    print_summary(size_json, tmp_path / "partitions.csv", tmp_path / "firmware.elf")


def test_print_summary_esp32_uses_dram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Original ESP32: RAM = DRAM.used / DRAM.total."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    _print_summary_ram_only(tmp_path, size_json)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "used 47332 bytes from 180736 bytes" in out


def test_print_summary_s3_falls_back_to_diram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """ESP32-S3 with no DRAM entry falls back to DIRAM and reports raw region usage."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    _print_summary_ram_only(tmp_path, size_json)
    out = capsys.readouterr().out
    assert "used 104999 bytes from 341760 bytes" in out


def test_print_summary_skips_when_diram_total_collapses(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A zero-size region drops the RAM line rather than divide by zero."""
    size_json = _write_size_json(
        tmp_path,
        {
            "version": "1.1",
            "layout": [{"name": "DIRAM", "total": 0, "used": 0}],
        },
    )
    _print_summary_ram_only(tmp_path, size_json)
    out = capsys.readouterr().out
    assert "RAM:" not in out
    assert "unusable region" in caplog.text


def test_print_summary_handles_missing_json(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Missing size json is non-fatal and prints nothing."""
    _print_summary_ram_only(tmp_path, tmp_path / "does_not_exist.json")
    assert capsys.readouterr().out == ""


def test_print_summary_handles_no_layout(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A size json without ``layout`` warns so schema drift is visible."""
    size_json = _write_size_json(tmp_path, {"version": "1.1"})
    _print_summary_ram_only(tmp_path, size_json)
    assert capsys.readouterr().out == ""
    assert any(
        r.levelname == "WARNING" and "no DRAM/DIRAM region" in r.message
        for r in caplog.records
    )


def test_print_summary_flash_line_prefers_total_size(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """With ``total_size`` in the json, that figure wins without reading the
    ELF, in the exact shape script/ci_memory_impact_extract.py greps."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = _write_partitions(tmp_path)
    print_summary(size_json, partitions, tmp_path / "firmware.elf")
    out = capsys.readouterr().out
    assert "Flash: " in out
    assert "(used 827455 bytes from 1835008 bytes)" in out


def test_print_summary_flash_line_derives_from_elf(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A 1.x json without ``total_size`` sums the ELF's loadable PROGBITS
    sections; NOBITS and non-alloc sections are excluded."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    partitions = _write_partitions(tmp_path)
    firmware_elf = tmp_path / "firmware.elf"
    firmware_elf.write_bytes(
        _elf_bytes(
            [
                (1, 0x6, 700000),  # PROGBITS, alloc+exec: counted
                (1, 0x2, 24215),  # PROGBITS, alloc: counted
                (8, 0x2, 50000),  # NOBITS (.bss): excluded
                (1, 0x0, 12345),  # PROGBITS, no alloc (.debug_*): excluded
            ]
        )
    )
    print_summary(size_json, partitions, firmware_elf)
    out = capsys.readouterr().out
    assert "(used 724215 bytes from 1835008 bytes)" in out


@pytest.mark.parametrize(
    "data",
    [
        pytest.param([1, 2], id="top_level_list"),
        pytest.param({"version": "1.1", "layout": None}, id="layout_null"),
        pytest.param({"version": "1.1", "layout": 7}, id="layout_scalar"),
    ],
)
def test_print_summary_handles_unexpected_shapes(
    data: object, tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A foreign-schema size json degrades to a warning, never a traceback."""
    size_json = _write_size_json(tmp_path, data)
    _print_summary_ram_only(tmp_path, size_json)
    assert capsys.readouterr().out == ""


def test_print_summary_skips_flash_on_zero_app_partition(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A zero-size app partition skips the Flash line rather than printing
    a from-0-bytes figure CI would record."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = tmp_path / "partitions.csv"
    partitions.write_text(
        "# name, type, subtype, offset, size, flags\napp0, app, ota_0, 0x10000, 0x0,\n"
    )
    print_summary(size_json, partitions, tmp_path / "firmware.elf")
    out = capsys.readouterr().out
    assert "Flash:" not in out


def test_print_summary_skips_flash_on_unreadable_partitions(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """An unreadable partitions.csv is non-fatal."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = _write_partitions(tmp_path)
    partitions.chmod(0o000)
    try:
        print_summary(size_json, partitions, tmp_path / "firmware.elf")
    finally:
        partitions.chmod(0o644)
    assert "Flash:" not in capsys.readouterr().out


def test_print_summary_flash_falls_back_on_bad_total_size(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A zero or non-int total_size falls back to the ELF instead of
    printing a used-0-bytes line CI would read as a real measurement."""
    data = _s3_size_data()
    data["total_size"] = 0
    size_json = _write_size_json(tmp_path, data)
    partitions = _write_partitions(tmp_path)
    firmware_elf = tmp_path / "firmware.elf"
    firmware_elf.write_bytes(_elf_bytes([(1, 0x2, 4096)]))
    print_summary(size_json, partitions, firmware_elf)
    out = capsys.readouterr().out
    assert "(used 4096 bytes from 1835008 bytes)" in out


_GOOD_ELF = _elf_bytes([(1, 0x2, 1024)])


@pytest.mark.parametrize(
    ("elf_bytes", "with_partitions"),
    [
        pytest.param(None, True, id="missing_elf"),
        pytest.param(b"junk", True, id="not_an_elf"),
        pytest.param(
            _elf_bytes([(1, 0x2, 1024)], shentsize=0), True, id="bad_shentsize"
        ),
        pytest.param(_GOOD_ELF[:60], True, id="truncated_table"),
        pytest.param(_elf_bytes([]), True, id="no_sections"),
        pytest.param(_elf_bytes([(8, 0x2, 50000)]), True, id="no_progbits"),
        pytest.param(_GOOD_ELF, False, id="missing_partitions"),
    ],
)
def test_print_summary_skips_flash_on_bad_input(
    elf_bytes: bytes | None,
    with_partitions: bool,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """An unusable ELF or missing partitions.csv skips the Flash line, not the RAM line."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    firmware_elf = tmp_path / "firmware.elf"
    if elf_bytes is not None:
        firmware_elf.write_bytes(elf_bytes)
    if with_partitions:
        _write_partitions(tmp_path)
    print_summary(size_json, tmp_path / "partitions.csv", firmware_elf)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "Flash:" not in out
    # ELF problems warn (anomaly after a successful build); a missing
    # partitions.csv stays at debug
    warned = any(
        r.levelname == "WARNING" and "Skipping Flash summary" in r.message
        for r in caplog.records
    )
    assert warned == with_partitions
