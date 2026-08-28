"""Tests for esphome.espidf.size_summary.print_summary."""

from __future__ import annotations

import json
from pathlib import Path

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
    """Call print_summary with no partitions.csv or firmware bin on disk."""
    print_summary(size_json, tmp_path / "partitions.csv", tmp_path / "firmware.bin")


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
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
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


def test_print_summary_handles_missing_json(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Missing size json is non-fatal and prints nothing."""
    _print_summary_ram_only(tmp_path, tmp_path / "does_not_exist.json")
    assert capsys.readouterr().out == ""


def test_print_summary_handles_no_layout(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A size json without ``layout`` still doesn't crash."""
    size_json = _write_size_json(tmp_path, {"version": "1.1"})
    _print_summary_ram_only(tmp_path, size_json)
    assert capsys.readouterr().out == ""


def test_print_summary_flash_line_prefers_total_size(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """With ``total_size`` in the json, the exact figure wins over the padded
    bin size, in the exact shape script/ci_memory_impact_extract.py greps."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = _write_partitions(tmp_path)
    firmware_bin = tmp_path / "firmware.bin"
    firmware_bin.write_bytes(b"\x00" * 999999)
    print_summary(size_json, partitions, firmware_bin)
    out = capsys.readouterr().out
    assert "Flash: " in out
    assert "(used 827455 bytes from 1835008 bytes)" in out


def test_print_summary_flash_line_falls_back_to_bin_size(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A 1.x json without ``total_size`` uses the on-disk bin size."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    partitions = _write_partitions(tmp_path)
    firmware_bin = tmp_path / "firmware.bin"
    firmware_bin.write_bytes(b"\x00" * 724224)
    print_summary(size_json, partitions, firmware_bin)
    out = capsys.readouterr().out
    assert "(used 724224 bytes from 1835008 bytes)" in out


@pytest.mark.parametrize("missing", ["bin", "partitions"])
def test_print_summary_skips_flash_on_missing_input(
    missing: str, tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A missing firmware bin or partitions.csv skips the Flash line, not the RAM line."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    firmware_bin = tmp_path / "firmware.bin"
    if missing == "bin":
        _write_partitions(tmp_path)
    else:
        firmware_bin.write_bytes(b"\x00" * 16)
    print_summary(size_json, tmp_path / "partitions.csv", firmware_bin)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "Flash:" not in out
