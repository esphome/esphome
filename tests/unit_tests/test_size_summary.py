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


def test_print_summary_flash_line(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """image_size + a factory app partition produce the Flash line."""
    size_json = tmp_path / "esp_idf_size.json"
    size_json.write_text(
        json.dumps(
            {
                "memory_types": {"DRAM": {"used": 100, "size": 200}},
                "image_size": 500,
            }
        )
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text(
        "# name, type, subtype, offset, size\napp0, app, factory, 0x10000, 0x100000\n"
    )
    print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "RAM:   [=====     ]  50.0% (used 100 bytes from 200 bytes)" in out
    assert "Flash: [          ]   0.0% (used 500 bytes from 1048576 bytes)" in out


def test_print_summary_missing_ram_region_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A missing RAM line is diagnosable, not a silently absent CI metric."""
    size_json = _write_size_json(tmp_path, {"memory_types": {}, "image_size": 100})
    print_summary(size_json, partitions_csv=None)
    assert "Skipping RAM summary" in caplog.text


def test_print_summary_bad_partitions_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unparseable partition table skips the Flash line with a warning."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("not,a,valid,partition,table\n")
    print_summary(size_json, partitions_csv=partitions)
    assert "Skipping Flash summary" in caplog.text


def test_print_summary_corrupt_json_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    size_json = tmp_path / "size.json"
    size_json.write_text("{not json")
    print_summary(size_json, partitions_csv=None)
    assert "Skipping size summary" in caplog.text


def test_print_summary_missing_flash_inputs_warn(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Both absent-input paths for the Flash line name their cause."""
    size_json = _write_size_json(tmp_path, _esp32_size_data())
    print_summary(size_json, partitions_csv=None)
    assert "no partition table given" in caplog.text
    caplog.clear()
    data = _esp32_size_data()
    data.pop("image_size", None)
    size_json = _write_size_json(tmp_path, data)
    print_summary(size_json, partitions_csv=tmp_path / "partitions.cssv")
    assert "no image_size" in caplog.text


def test_print_summary_non_dict_json_warns(tmp_path, caplog) -> None:
    """Valid JSON that is not an object must warn, not raise past a build
    that already linked."""
    size_json = tmp_path / "size.json"
    size_json.write_text("[]")
    print_summary(size_json, tmp_path / "partitions.csv")
    assert "unexpected shape" in caplog.text


def test_print_summary_zero_app_partition_warns(tmp_path, caplog) -> None:
    """A partition row with size 0 drops the Flash bar instead of rendering 0%."""
    size_json = tmp_path / "size.json"
    size_json.write_text(
        '{"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100}'
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, 0,\n")
    print_summary(size_json, partitions)
    assert "app partition size is" in caplog.text


def test_print_summary_blank_partition_size_warns(tmp_path, caplog) -> None:
    """A blank size cell raises ValueError by name instead of parsing to 0."""
    size_json = tmp_path / "size.json"
    size_json.write_text(
        '{"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100}'
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, ,\n")
    print_summary(size_json, partitions)
    assert "blank partition size cell" in caplog.text


@pytest.mark.parametrize(
    "payload",
    (
        '{"memory_types": []}',
        '{"memory_types": {"DRAM": 5}}',
        '{"memory_types": {"DRAM": {"used": "x", "size": "y"}}}',
    ),
    ids=("non-dict-memory-types", "scalar-region", "non-numeric-sizes"),
)
def test_print_summary_nested_shapes_never_raise(tmp_path, caplog, payload) -> None:
    """The blanket guard keeps unexpected nested shapes from raising."""
    size_json = tmp_path / "size.json"
    size_json.write_text(payload)
    print_summary(size_json, tmp_path / "partitions.csv")
    assert "Skipping size summary" in caplog.text
