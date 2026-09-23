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
    """Synthetic esp_idf_size.json for the original ESP32 (split IRAM/DRAM)."""
    return {
        "image_size": 827455,
        "memory_types": {
            "DRAM": {
                "size": 180736,
                "used": 47332,
                "sections": {
                    ".dram0.bss": {"abbrev_name": ".bss", "size": 30616},
                    ".dram0.data": {"abbrev_name": ".data", "size": 16716},
                },
            },
            "IRAM": {
                "size": 131072,
                "used": 80351,
                "sections": {
                    ".iram0.text": {"abbrev_name": ".text", "size": 79323},
                    ".iram0.vectors": {"abbrev_name": ".vectors", "size": 1028},
                },
            },
        },
    }


def _s3_size_data() -> dict:
    """Synthetic esp_idf_size.json for ESP32-S3 (unified DIRAM)."""
    return {
        "image_size": 724215,
        "memory_types": {
            "DIRAM": {
                "size": 341760,
                "used": 104999,
                "sections": {
                    ".iram0.text": {"abbrev_name": ".text", "size": 58051},
                    ".dram0.bss": {"abbrev_name": ".bss", "size": 27088},
                    ".dram0.data": {"abbrev_name": ".data", "size": 19708},
                    ".noinit": {"abbrev_name": ".noinit", "size": 152},
                },
            },
            "IRAM": {
                "size": 16384,
                "used": 16384,
                "sections": {
                    ".iram0.text": {"abbrev_name": ".text", "size": 15356},
                    ".iram0.vectors": {"abbrev_name": ".vectors", "size": 1028},
                },
            },
        },
    }


def test_print_summary_esp32_uses_dram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Original ESP32: DRAM has no ``.text``, so RAM = DRAM.used / DRAM.size unchanged."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    print_summary(size_json, partitions_csv=None)
    out = capsys.readouterr().out
    assert "RAM:" in out
    assert "used 47332 bytes from 180736 bytes" in out


def test_print_summary_s3_falls_back_to_diram(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """ESP32-S3 with no DRAM key falls back to DIRAM and reports raw region usage."""
    size_json = _write_size_json(tmp_path, _s3_size_data())
    print_summary(size_json, partitions_csv=None)
    out = capsys.readouterr().out
    assert "used 104999 bytes from 341760 bytes" in out


def test_print_summary_skips_when_diram_total_collapses(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A zero-size region drops the RAM line rather than divide by zero."""
    size_json = _write_size_json(
        tmp_path,
        {
            "memory_types": {
                "DIRAM": {
                    "size": 0,
                    "used": 0,
                    "sections": {},
                },
            },
        },
    )
    print_summary(size_json, partitions_csv=None)
    out = capsys.readouterr().out
    assert "RAM:" not in out


def test_print_summary_handles_missing_json(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Missing size json is non-fatal and prints nothing."""
    print_summary(tmp_path / "does_not_exist.json", partitions_csv=None)
    assert capsys.readouterr().out == ""


def test_print_summary_handles_no_memory_types(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A size json without ``memory_types`` still doesn't crash."""
    size_json = _write_size_json(tmp_path, {"image_size": 0})
    print_summary(size_json, partitions_csv=None)
    assert capsys.readouterr().out == ""
