"""Tests for esphome.espidf.size_summary.print_summary."""

from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import patch

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


def test_print_summary_non_dict_json_is_skipped(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Valid JSON that is not an object must not raise past a linked build."""
    size_json = tmp_path / "size.json"
    size_json.write_text("[]")
    print_summary(size_json, tmp_path / "partitions.csv")
    assert capsys.readouterr().out == ""


def test_print_summary_unreadable_partitions_is_skipped(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """An OSError reading the partition table skips the summary, not the build."""
    size_json = tmp_path / "size.json"
    size_json.write_text(
        '{"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100}'
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, 1M,\n")
    real_read_text = Path.read_text

    def fail_partitions_read(self: Path, *args: object, **kwargs: object) -> str:
        if self == partitions:
            raise OSError("permission denied")
        return real_read_text(self, *args, **kwargs)

    with patch.object(Path, "read_text", fail_partitions_read):
        print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "RAM:" in out and "Flash:" not in out


def test_print_summary_zero_app_partition_is_skipped(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A 0-size app partition drops the Flash bar instead of rendering 0%."""
    size_json = tmp_path / "size.json"
    size_json.write_text(
        '{"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100}'
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, 0,\n")
    print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "RAM:" in out and "Flash:" not in out


def test_print_summary_happy_path_prints_both_bars(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A well-formed size report and partition table print both bars."""
    size_json = tmp_path / "size.json"
    size_json.write_text(
        '{"memory_types": {"DRAM": {"used": 1000, "size": 2000}}, "image_size": 100000}'
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, 0x180000,\n")
    print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "RAM:" in out and "Flash:" in out


@pytest.mark.parametrize(
    "payload",
    [
        {"memory_types": []},
        {"memory_types": {"DRAM": 5}},
        {"memory_types": {"DRAM": {"used": "x", "size": "y"}}, "image_size": 1},
    ],
)
def test_print_summary_nested_bad_shapes_never_raise(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
    payload: dict,
) -> None:
    """Bad nested shapes hit the named RAM guard, not the blanket backstop."""
    size_json = _write_size_json(tmp_path, payload)
    print_summary(size_json, None)
    # No half-formed bar for CI to scrape; every payload fails before printing
    assert capsys.readouterr().out == ""
    assert "Skipping RAM summary" in caplog.text
    assert "Skipping size summary for" not in caplog.text


def test_print_summary_blanket_guard_catches_the_rest(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Shapes the named guards miss (non-numeric image_size) warn via the
    blanket backstop, and the buffered report prints nothing at all."""
    size_json = _write_size_json(
        tmp_path,
        {"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": "x"},
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, 0x100000,\n")
    print_summary(size_json, partitions)
    assert capsys.readouterr().out == ""
    assert "Skipping size summary for" in caplog.text


def test_print_summary_blank_size_cell_names_the_row(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A blank size cell raises ValueError instead of parsing to 0."""
    size_json = _write_size_json(
        tmp_path,
        {"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100},
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("app0, app, ota_0, 0x10000, ,\n")
    print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "RAM:" in out and "Flash:" not in out
    # Pins the ValueError path: pre-diff, "" parsed to 0 and the size-0
    # warning fired instead
    assert "blank partition size cell" in caplog.text
    assert "app0" in caplog.text and str(partitions) in caplog.text


def test_print_summary_suffixed_size_cell(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """K/M suffixes parse like PlatformIO's rule (1M = 1048576 bytes)."""
    size_json = _write_size_json(
        tmp_path,
        {"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100},
    )
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("# comment row\nshort,row\napp0, app, ota_0, 0x10000, 1M,\n")
    print_summary(size_json, partitions)
    assert "from 1048576 bytes" in capsys.readouterr().out


def test_print_summary_missing_or_appless_partitions_stay_quiet(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A missing table or one without a qualifying app row is a legitimate
    layout: the Flash line drops at debug, never at warning."""
    size_json = _write_size_json(
        tmp_path,
        {"memory_types": {"DRAM": {"used": 1, "size": 2}}, "image_size": 100},
    )
    print_summary(size_json, tmp_path / "nope.csv")
    partitions = tmp_path / "partitions.csv"
    partitions.write_text("st0, data, spiffs, 0x10000, 0x1000,\n")
    print_summary(size_json, partitions)
    out = capsys.readouterr().out
    assert "Flash:" not in out
    assert "Skipping Flash summary" not in caplog.text


def test_print_summary_corrupt_size_json_warns(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The build's own size report failing to parse is a regression signal."""
    size_json = tmp_path / "size.json"
    size_json.write_text("not json {{{")
    print_summary(size_json, None)
    assert capsys.readouterr().out == ""
    assert "Skipping size summary" in caplog.text
