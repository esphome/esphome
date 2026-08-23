"""Unit tests for run_codechecker_zephyr() in script/clang-tidy."""

from __future__ import annotations

import argparse
import importlib.machinery
import importlib.util
import json
from pathlib import Path
import sys
from unittest.mock import MagicMock

import pytest

script_dir = str((Path(__file__).parent / ".." / ".." / "script").resolve())
sys.path.insert(0, script_dir)
_loader = importlib.machinery.SourceFileLoader(
    "clang_tidy_script", str(Path(script_dir) / "clang-tidy")
)
spec = importlib.util.spec_from_file_location(
    "clang_tidy_script", _loader.path, loader=_loader
)
clang_tidy_script = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clang_tidy_script)

import clang_tidy_hash  # noqa: E402

# Captured before _common_mocks (autouse) stubs out the module attribute, so
# these still call the real implementation regardless of that patch.
_get_codechecker_binary = clang_tidy_script._get_codechecker_binary


def _codechecker_version_stdout(analyzer_version: str, web_version: str) -> str:
    """Mimic CodeChecker's real `version` output: two independently-versioned
    sections (analyzer package, web package) sharing the word "version"."""
    return (
        "CodeChecker analyzer version:\n"
        f"Base package version | {analyzer_version}\n"
        f"Git tag information  | {analyzer_version}\n"
        "\n"
        "CodeChecker web version:\n"
        f"Base package version                | {web_version}\n"
        f"Git tag information                 | {web_version}\n"
        "Server supported Thrift API version | 6.71\n"
    )


def _args(**overrides: object) -> argparse.Namespace:
    ns = argparse.Namespace(environment="nrf52-adafruit", jobs=1, fix=False)
    ns.__dict__.update(overrides)
    return ns


def _write_metadata(output_dir: Path, successful: int, failed: int) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    metadata = {
        "tools": [
            {
                "analyzers": {
                    "clang-tidy": {
                        "analyzer_statistics": {
                            "successful": successful,
                            "failed": failed,
                        }
                    }
                }
            }
        ]
    }
    (output_dir / "metadata.json").write_text(json.dumps(metadata))


@pytest.fixture
def explicit_compile_commands(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """Bypass compile-command generation via ESPHOME_ZEPHYR_COMPILE_COMMANDS."""
    cc = tmp_path / "compile_commands.json"
    cc.write_text("[]")
    monkeypatch.setenv("ESPHOME_ZEPHYR_COMPILE_COMMANDS", str(cc))
    return cc


@pytest.fixture(autouse=True)
def _common_mocks(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    # Tests without explicit_compile_commands need this unset to hit the
    # generate-and-cache branch, regardless of the ambient shell.
    monkeypatch.delenv("ESPHOME_ZEPHYR_COMPILE_COMMANDS", raising=False)
    monkeypatch.setattr(clang_tidy_script, "temp_folder", str(tmp_path))
    monkeypatch.setattr(
        clang_tidy_script, "_get_codechecker_binary", lambda *a, **k: "CodeChecker"
    )


def test_get_codechecker_binary_accepts_matching_pin(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    stdout = _codechecker_version_stdout(
        analyzer_version="6.28.2", web_version="6.28.2"
    )
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout),
    )
    assert _get_codechecker_binary("6.28") == "CodeChecker"


def test_get_codechecker_binary_rejects_field_collision(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The pinned digits appearing in the *web* section must not validate a
    mismatched *analyzer* version (Koan finding #6 -- the bug this exists to fix)."""
    stdout = _codechecker_version_stdout(
        analyzer_version="6.30.0", web_version="6.28.2"
    )
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout),
    )
    with pytest.raises(RuntimeError, match="6.30"):
        _get_codechecker_binary("6.28")


def test_get_codechecker_binary_raises_when_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def fake_run(*a, **k):
        raise FileNotFoundError("CodeChecker")

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    with pytest.raises(FileNotFoundError):
        _get_codechecker_binary("6.28")


def test_get_codechecker_binary_raises_on_unparseable_output(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout="unexpected garbage output"),
    )
    with pytest.raises(RuntimeError, match="unknown"):
        _get_codechecker_binary("6.28")


def test_run_codechecker_zephyr_success(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    assert result == 0


def test_run_codechecker_zephyr_fails_on_parse_failure(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A non-zero `parse` exit is the sole gate for real findings and must fail the run."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
            return MagicMock(returncode=0)
        if cmd[1] == "parse":
            return MagicMock(returncode=2)  # CodeChecker parse: 2 == reports found
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    assert result == 1
    tidy_args = (output_dir / "tidy_args.txt").read_text()
    assert "--warnings-as-errors=-*" in tidy_args


def test_run_codechecker_zephyr_fails_when_any_file_failed(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A crashed/errored file must not read as a clean run (Koan finding #7)."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            # Old buggy check summed successful+failed == len(files) and passed.
            _write_metadata(output_dir, successful=0, failed=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    assert result == 1


def test_run_codechecker_zephyr_fails_on_file_count_mismatch(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=0, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    assert result == 1


def test_run_codechecker_zephyr_fails_on_fixit_failure(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A failed `fixit --apply` must not be silently discarded (Koan finding #6)."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
            return MagicMock(returncode=0)
        if cmd[1] == "fixit":
            assert kwargs.get("stdin") is clang_tidy_script.subprocess.DEVNULL
            return MagicMock(returncode=3)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args(fix=True))
    assert result == 1


def test_run_codechecker_zephyr_does_not_cache_on_mismatch(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Cache must only be marked fresh once validated (Koan finding #8)."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(work_dir, platformio_ini, source_files):
        cc_path.parent.mkdir(parents=True, exist_ok=True)
        cc_path.write_text("[]")
        return cc_path

    monkeypatch.setattr(
        "esphome.components.nrf52.clang_tidy.generate_compile_commands",
        fake_generate,
    )
    monkeypatch.setattr(clang_tidy_hash, "is_cached", lambda *a, **k: False)
    update_cache_calls = []
    monkeypatch.setattr(
        clang_tidy_hash,
        "update_cache",
        lambda *a, **k: update_cache_calls.append((a, k)),
    )

    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            # Mismatch: 0 successful for 1 requested file.
            _write_metadata(output_dir, successful=0, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert update_cache_calls == []


def test_run_codechecker_zephyr_caches_once_validated(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(work_dir, platformio_ini, source_files):
        cc_path.parent.mkdir(parents=True, exist_ok=True)
        cc_path.write_text("[]")
        return cc_path

    monkeypatch.setattr(
        "esphome.components.nrf52.clang_tidy.generate_compile_commands",
        fake_generate,
    )
    monkeypatch.setattr(clang_tidy_hash, "is_cached", lambda *a, **k: False)
    update_cache_calls = []
    monkeypatch.setattr(
        clang_tidy_hash,
        "update_cache",
        lambda *a, **k: update_cache_calls.append((a, k)),
    )

    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd, **kwargs):
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 0
    assert len(update_cache_calls) == 1
