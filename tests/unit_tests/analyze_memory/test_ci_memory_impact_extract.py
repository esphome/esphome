"""Tests for script/ci_memory_impact_extract.py."""

import io
from pathlib import Path
import sys

import pytest

# Add script directory to path so we can import the module
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / "script"))

from ci_memory_impact_extract import main  # noqa: E402

_COMPILE_OUTPUT = (
    "RAM:   [====      ]  36.1% (used 29548 bytes from 81920 bytes)\n"
    "Flash: [===       ]  34.0% (used 348511 bytes from 1023984 bytes)\n"
)


@pytest.fixture(autouse=True)
def _no_github_output(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("GITHUB_OUTPUT", raising=False)


def _run(monkeypatch: pytest.MonkeyPatch, compile_output: str, argv: list[str]) -> int:
    monkeypatch.setattr(sys, "stdin", io.StringIO(compile_output))
    monkeypatch.setattr(sys, "argv", ["ci_memory_impact_extract.py", *argv])
    return main()


def test_missing_detailed_analysis_fails(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A build with no usable ELF fails instead of posting a comment without detail."""
    build_dir = tmp_path / ".esphome" / "build" / "mydevice"
    build_dir.mkdir(parents=True)
    out_json = tmp_path / "analysis.json"

    rc = _run(
        monkeypatch,
        _COMPILE_OUTPUT,
        ["--build-dir", str(build_dir), "--output-json", str(out_json)],
    )

    assert rc == 1
    # The totals are still written so the failure can be diagnosed from the artifact
    assert out_json.is_file()


def test_undetected_build_dir_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    """Compile output without a build path cannot be analyzed, so it fails."""
    assert _run(monkeypatch, _COMPILE_OUTPUT, []) == 1


def test_unparseable_output_fails(monkeypatch: pytest.MonkeyPatch) -> None:
    """Output with no memory totals at all is still a failure."""
    assert _run(monkeypatch, "nothing useful here\n", []) == 1
