"""Unit tests for script/check_import_time.py."""

from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import sys
from unittest.mock import patch

import pytest

# Load the script-under-test as `check_import_time` (it's a hyphenated path
# inside `script/` that mirrors the existing `determine_jobs` pattern).
script_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "script")
)
sys.path.insert(0, script_dir)
spec = importlib.util.spec_from_file_location(
    "check_import_time", os.path.join(script_dir, "check_import_time.py")
)
check_import_time = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_import_time)


def _entry(name: str, self_us: int, cumulative_us: int) -> dict:
    """Build a minimal HAR entry matching `importtime_waterfall --har`."""
    return {
        "request": {"url": name},
        "time": cumulative_us,
        "timings": {"receive": self_us, "wait": cumulative_us - self_us},
    }


def _har(*entries: dict) -> dict:
    return {"log": {"entries": list(entries)}}


def test_root_cumulative_us_returns_time_for_root_module() -> None:
    har = _har(
        _entry("dep_a", 500, 500),
        _entry("dep_b", 300, 300),
        _entry("esphome.__main__", 100, 1000),
    )
    assert check_import_time.root_cumulative_us(har, "esphome.__main__") == 1000


def test_root_cumulative_us_missing_module_raises() -> None:
    har = _har(_entry("something.else", 100, 100))
    with pytest.raises(RuntimeError, match="No HAR entry for 'esphome.__main__'"):
        check_import_time.root_cumulative_us(har, "esphome.__main__")


def test_top_offenders_ranks_by_self_time_descending() -> None:
    har = _har(
        _entry("small", 100, 100),
        _entry("big", 5000, 5000),
        _entry("medium", 2000, 2500),
    )
    result = check_import_time.top_offenders(har, n=10)
    assert [name for name, _, _ in result] == ["big", "medium", "small"]
    assert result[0] == ("big", 5000, 5000)


def test_top_offenders_respects_n_limit() -> None:
    har = _har(*[_entry(f"m{i}", i * 100, i * 100) for i in range(1, 20)])
    assert len(check_import_time.top_offenders(har, n=5)) == 5


def test_top_offenders_dedupes_repeat_names_keeping_first() -> None:
    har = _har(
        _entry("pkg", 5000, 5000),
        _entry("pkg", 100, 100),  # reimport later in trace
        _entry("other", 1000, 1000),
    )
    result = check_import_time.top_offenders(har, n=10)
    assert [name for name, _, _ in result] == ["pkg", "other"]
    # First occurrence wins
    assert ("pkg", 5000, 5000) in result


def test_format_us_switches_to_ms_at_threshold() -> None:
    assert check_import_time._format_us(500) == "500us"
    assert check_import_time._format_us(999) == "999us"
    assert check_import_time._format_us(1000) == "1.0ms"
    assert check_import_time._format_us(12345) == "12.3ms"


def test_read_write_budget_roundtrip(tmp_path: Path) -> None:
    budget_path = tmp_path / "budget.json"
    with patch.object(check_import_time, "BUDGET_PATH", budget_path):
        assert check_import_time.read_budget() == {}
        check_import_time.write_budget(cumulative_us=12345, margin_pct=20)
        loaded = check_import_time.read_budget()
    assert loaded["cumulative_us"] == 12345
    assert loaded["margin_pct"] == 20
    assert loaded["target_module"] == check_import_time.TARGET_MODULE


def test_cmd_check_passes_when_measured_within_ceiling(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    budget_path = tmp_path / "budget.json"
    budget_path.write_text(
        json.dumps(
            {
                "target_module": check_import_time.TARGET_MODULE,
                "margin_pct": 15,
                "cumulative_us": 100000,  # 100ms
            }
        )
    )
    # Measured 90ms: inside 100ms + 15% = 115ms ceiling
    har = _har(_entry(check_import_time.TARGET_MODULE, 1000, 90000))
    args = type("A", (), {"har": None})()
    with (
        patch.object(check_import_time, "BUDGET_PATH", budget_path),
        patch.object(check_import_time, "measure", return_value=har),
    ):
        rc = check_import_time.cmd_check(args)
    assert rc == 0
    out = capsys.readouterr().out
    assert "measured   esphome.__main__:" in out
    assert "budget 100.0ms" in out


def test_cmd_check_fails_when_measured_exceeds_ceiling(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    budget_path = tmp_path / "budget.json"
    budget_path.write_text(
        json.dumps(
            {
                "target_module": check_import_time.TARGET_MODULE,
                "margin_pct": 15,
                "cumulative_us": 100000,
            }
        )
    )
    # Measured 120ms: over 100ms + 15% = 115ms ceiling
    har = _har(
        _entry("offender_a", 10000, 10000),
        _entry(check_import_time.TARGET_MODULE, 1000, 120000),
    )
    args = type("A", (), {"har": None})()
    with (
        patch.object(check_import_time, "BUDGET_PATH", budget_path),
        patch.object(check_import_time, "measure", return_value=har),
    ):
        rc = check_import_time.cmd_check(args)
    assert rc == 1
    err = capsys.readouterr().err
    assert "REGRESSION" in err
    assert "120.0ms" in err
    assert "offender_a" in err  # top offender table


def test_cmd_check_returns_2_when_budget_missing(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    budget_path = tmp_path / "nonexistent.json"
    args = type("A", (), {"har": None})()
    with patch.object(check_import_time, "BUDGET_PATH", budget_path):
        rc = check_import_time.cmd_check(args)
    assert rc == 2
    assert "missing" in capsys.readouterr().err


def test_cmd_check_writes_har_when_path_given(tmp_path: Path) -> None:
    budget_path = tmp_path / "budget.json"
    budget_path.write_text(
        json.dumps(
            {
                "target_module": check_import_time.TARGET_MODULE,
                "margin_pct": 15,
                "cumulative_us": 100000,
            }
        )
    )
    har_path = tmp_path / "out.har"
    har_text = json.dumps(_har(_entry(check_import_time.TARGET_MODULE, 1000, 80000)))
    args = type("A", (), {"har": str(har_path)})()
    with (
        patch.object(check_import_time, "BUDGET_PATH", budget_path),
        patch.object(check_import_time, "run_waterfall", return_value=har_text),
    ):
        rc = check_import_time.cmd_check(args)
    assert rc == 0
    assert har_path.exists()
    assert json.loads(har_path.read_text()) == json.loads(har_text)
