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


def _esp32_size_data() -> dict:
    """Synthetic json2 esp_idf_size.json for the original ESP32 (split IRAM/DRAM)."""
    return {
        "version": "1.1",
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
    """Synthetic json2 esp_idf_size.json for ESP32-S3 (unified DIRAM)."""
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


def test_print_summary_esp32_uses_dram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Original ESP32: RAM = DRAM.used / DRAM.total."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    print_summary(size_json, partitions_csv=None, firmware_bin=None)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "used 47332 bytes from 180736 bytes" in out


def test_print_summary_s3_falls_back_to_diram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """ESP32-S3 with no DRAM entry falls back to DIRAM and reports raw region usage."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    print_summary(size_json, partitions_csv=None, firmware_bin=None)
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
            "layout": [
                {
                    "name": "DIRAM",
                    "total": 0,
                    "used": 0,
                    "free": 0,
                    "parts": {},
                },
            ],
        },
    )
    print_summary(size_json, partitions_csv=None, firmware_bin=None)
    out = capsys.readouterr().out
    assert "RAM:" not in out


def test_print_summary_handles_missing_json(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Missing size json is non-fatal and prints nothing."""
    print_summary(
        tmp_path / "does_not_exist.json", partitions_csv=None, firmware_bin=None
    )
    assert capsys.readouterr().out == ""


def test_print_summary_handles_no_layout(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A size json without ``layout`` still doesn't crash."""
    size_json = _write_size_json(tmp_path, {"version": "1.1"})
    print_summary(size_json, partitions_csv=None, firmware_bin=None)
    assert capsys.readouterr().out == ""


def test_print_summary_flash_line(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A partition table with an app row yields the Flash line in the exact
    padded shape script/ci_memory_impact_extract.py greps."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = tmp_path / "partitions.csv"
    partitions.write_text(
        "# name, type, subtype, offset, size, flags\n"
        "app0, app, ota_0, 0x10000, 0x1C0000,\n"
    )
    firmware_bin = tmp_path / "firmware.bin"
    firmware_bin.write_bytes(b"\x00" * 827455)
    print_summary(size_json, partitions, firmware_bin)
    out = capsys.readouterr().out
    assert "Flash: " in out
    assert "(used 827455 bytes from 1835008 bytes)" in out


def test_print_summary_skips_flash_without_bin(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """No firmware bin means the RAM line prints but the Flash line is skipped."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = tmp_path / "partitions.csv"
    partitions.write_text(
        "# name, type, subtype, offset, size, flags\n"
        "app0, app, ota_0, 0x10000, 0x1C0000,\n"
    )
    print_summary(size_json, partitions, firmware_bin=None)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "Flash:" not in out
