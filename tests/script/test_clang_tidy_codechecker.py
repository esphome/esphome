"""Unit tests for run_codechecker_zephyr() in script/clang-tidy."""

from __future__ import annotations

import argparse
import importlib.machinery
import importlib.util
import json
from pathlib import Path
import re
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
_verify_codechecker_clang_tidy = clang_tidy_script._verify_codechecker_clang_tidy


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


def _write_metadata(
    output_dir: Path, successful: int, failed: int, source: str = "file.cpp"
) -> None:
    """Write a metadata.json reporting 0 or 1 successful/failed analyses of ``source``.

    Every caller here analyzes a single requested file, so successful/failed
    are each 0 or 1; successful_sources/failed_sources are derived from that
    so the new per-file coverage check (analyzed_sources vs requested files)
    matches what the count implies.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    resolved_source = str(Path(source).resolve())
    metadata = {
        "tools": [
            {
                "analyzers": {
                    "clang-tidy": {
                        "analyzer_statistics": {
                            "successful": successful,
                            "failed": failed,
                            "successful_sources": [resolved_source] * successful,
                            "failed_sources": [resolved_source] * failed,
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
    monkeypatch.setattr(
        clang_tidy_script, "_verify_codechecker_clang_tidy", lambda *a, **k: None
    )


def test_codechecker_pinned_version_parses_requirements_file() -> None:
    major, minor = clang_tidy_script._codechecker_pinned_version()
    assert (major, minor) > (0, -1)


def test_get_codechecker_binary_accepts_matching_pin(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    pin = clang_tidy_script._codechecker_pinned_version()
    version = f"{pin[0]}.{pin[1]}.2"
    stdout = _codechecker_version_stdout(analyzer_version=version, web_version=version)
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout, stderr=""),
    )
    assert _get_codechecker_binary(pin) == "CodeChecker"


def test_get_codechecker_binary_accepts_newer_minor(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Minor version is soft-pinned: newer minors within the same major are accepted."""
    pin = clang_tidy_script._codechecker_pinned_version()
    version = f"{pin[0]}.{pin[1] + 2}.0"
    stdout = _codechecker_version_stdout(analyzer_version=version, web_version=version)
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout, stderr=""),
    )
    assert _get_codechecker_binary(pin) == "CodeChecker"


def test_get_codechecker_binary_rejects_older_minor(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    pin = clang_tidy_script._codechecker_pinned_version()
    if pin[1] == 0:
        pytest.skip("pinned minor is 0, no older minor within the same major exists")
    version = f"{pin[0]}.{pin[1] - 1}.9"
    stdout = _codechecker_version_stdout(analyzer_version=version, web_version=version)
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout, stderr=""),
    )
    with pytest.raises(RuntimeError, match=f"{pin[0]}.{pin[1] - 1}"):
        _get_codechecker_binary(pin)


def test_get_codechecker_binary_rejects_field_collision(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The pinned digits appearing in the *web* section must not validate a
    mismatched *analyzer* version."""
    pin = clang_tidy_script._codechecker_pinned_version()
    mismatched_major = pin[0] - 1
    stdout = _codechecker_version_stdout(
        analyzer_version=f"{mismatched_major}.30.0",
        web_version=f"{pin[0]}.{pin[1]}.2",
    )
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout, stderr=""),
    )
    with pytest.raises(RuntimeError, match=f"{mismatched_major}.30"):
        _get_codechecker_binary(pin)


def test_get_codechecker_binary_rejects_newer_major(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Major version is hard-pinned: a newer major is never accepted."""
    pin = clang_tidy_script._codechecker_pinned_version()
    newer_major = pin[0] + 1
    version = f"{newer_major}.0.0"
    stdout = _codechecker_version_stdout(analyzer_version=version, web_version=version)
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=stdout, stderr=""),
    )
    with pytest.raises(RuntimeError, match=f"{newer_major}.0"):
        _get_codechecker_binary(pin)


def test_get_codechecker_binary_raises_when_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def fake_run(*a: object, **k: object) -> None:
        raise FileNotFoundError("CodeChecker")

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    with pytest.raises(FileNotFoundError):
        _get_codechecker_binary(clang_tidy_script._codechecker_pinned_version())


def test_get_codechecker_binary_raises_on_unparseable_output(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(
            returncode=0, stdout="unexpected garbage output", stderr=""
        ),
    )
    with pytest.raises(RuntimeError, match="unknown"):
        _get_codechecker_binary(clang_tidy_script._codechecker_pinned_version())


def test_get_codechecker_binary_includes_stderr_in_error(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A broken CodeChecker install must surface its real diagnostic, not just
    "version unknown"."""
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(
            returncode=1,
            stdout="",
            stderr="ImportError: No module named 'thrift'\n",
        ),
    )
    with pytest.raises(RuntimeError, match="ImportError: No module named 'thrift'"):
        _get_codechecker_binary(clang_tidy_script._codechecker_pinned_version())


def test_clang_tidy_pinned_major_parses_requirements_file() -> None:
    assert clang_tidy_script._clang_tidy_pinned_major() > 0


def test_verify_codechecker_clang_tidy_accepts_matching_major(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    major = clang_tidy_script._clang_tidy_pinned_major()
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=0, stdout=f"LLVM version {major}.1.8\n"),
    )
    _verify_codechecker_clang_tidy(major)


def test_verify_codechecker_clang_tidy_rejects_mismatched_major(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    major = clang_tidy_script._clang_tidy_pinned_major()
    other_major = major + 1
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(
            returncode=0, stdout=f"LLVM version {other_major}.0.0\n"
        ),
    )
    with pytest.raises(RuntimeError, match=str(other_major)):
        _verify_codechecker_clang_tidy(major)


def test_verify_codechecker_clang_tidy_rejects_unparseable_output(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(
        clang_tidy_script.subprocess,
        "run",
        lambda *a, **k: MagicMock(returncode=127, stdout=""),
    )
    with pytest.raises(RuntimeError, match="unknown"):
        _verify_codechecker_clang_tidy(clang_tidy_script._clang_tidy_pinned_major())


def test_verify_codechecker_clang_tidy_raises_when_missing(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    def fake_run(*a: object, **k: object) -> None:
        raise FileNotFoundError("clang-tidy")

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    with pytest.raises(FileNotFoundError):
        _verify_codechecker_clang_tidy(clang_tidy_script._clang_tidy_pinned_major())


def test_clang_tidy_pinned_major_raises_when_pin_missing(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    (tmp_path / "requirements_dev.txt").write_text("clang-format==13.0.1\n")
    monkeypatch.setattr(clang_tidy_script, "root_path", str(tmp_path))
    with pytest.raises(RuntimeError, match="clang-tidy"):
        clang_tidy_script._clang_tidy_pinned_major()


def test_run_codechecker_zephyr_reports_missing_clang_tidy_distinctly(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """clang-tidy missing must be reported as clang-tidy missing, not CodeChecker
    missing -- regression test for the two checks sharing one except clause."""
    monkeypatch.setattr(
        clang_tidy_script,
        "_verify_codechecker_clang_tidy",
        _verify_codechecker_clang_tidy,
    )

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        raise FileNotFoundError(2, "No such file or directory", "clang-tidy")

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    stderr = capsys.readouterr().err
    assert "clang-tidy is not installed" in stderr
    assert "CodeChecker is not installed" not in stderr


def test_run_codechecker_zephyr_reports_missing_codechecker_binary(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """Exercises _get_codechecker_binary's real call site in run_codechecker_zephyr --
    the autouse stub would hide an argument-count regression there otherwise."""
    monkeypatch.setattr(
        clang_tidy_script, "_get_codechecker_binary", _get_codechecker_binary
    )

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        raise FileNotFoundError(2, "No such file or directory", "CodeChecker")

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    stderr = capsys.readouterr().err
    assert "CodeChecker is not installed" in stderr


def test_run_codechecker_zephyr_reports_pin_lookup_failure_distinctly(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """A missing/renamed requirements file must not be misreported as a missing
    CodeChecker/clang-tidy binary -- regression test for the pin lookups
    previously running inside the binary-presence try blocks."""

    def fake_pinned_major() -> int:
        raise FileNotFoundError("requirements_dev.txt")

    monkeypatch.setattr(
        clang_tidy_script, "_clang_tidy_pinned_major", fake_pinned_major
    )
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    stderr = capsys.readouterr().err
    assert "Could not determine pinned tool versions" in stderr
    assert "CodeChecker is not installed" not in stderr
    assert "clang-tidy is not installed" not in stderr


def test_run_codechecker_zephyr_success(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
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

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
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
    tmp_path: Path,
    explicit_compile_commands: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A crashed/errored file must not read as a clean run."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            # Old buggy check summed successful+failed == len(files) and passed.
            _write_metadata(output_dir, successful=0, failed=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    # The failing file must be named, not just counted -- otherwise an
    # analyzer crash in CI is undiagnosable from the log.
    assert str(Path("file.cpp").resolve()) in capsys.readouterr().err
    assert result == 1


def test_run_codechecker_zephyr_fails_on_file_count_mismatch(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=0, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())
    assert result == 1


def test_run_codechecker_zephyr_fails_when_counts_match_but_a_file_is_missing(
    tmp_path: Path,
    explicit_compile_commands: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A --file glob over-matching one unrequested file while another requested
    file matches nothing must not cancel out in a count-only comparison."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"
    unrequested_source = str((tmp_path / "unrequested.cpp").resolve())
    analyzed_source = str(Path("a.cpp").resolve())

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            output_dir.mkdir(parents=True, exist_ok=True)
            metadata = {
                "tools": [
                    {
                        "analyzers": {
                            "clang-tidy": {
                                "analyzer_statistics": {
                                    "successful": 2,
                                    "failed": 0,
                                    # b.cpp was never analyzed; unrequested_source keeps the count at 2 anyway.
                                    "successful_sources": [
                                        analyzed_source,
                                        unrequested_source,
                                    ],
                                    "failed_sources": [],
                                }
                            }
                        }
                    }
                ]
            }
            (output_dir / "metadata.json").write_text(json.dumps(metadata))
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["a.cpp", "b.cpp"], _args())

    assert result == 1
    stderr = capsys.readouterr().err
    assert str(Path("b.cpp").resolve()) in stderr
    assert str(Path("a.cpp").resolve()) not in stderr


def test_run_codechecker_zephyr_fails_when_an_unrequested_file_is_analyzed(
    tmp_path: Path,
    explicit_compile_commands: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """An over-matching --file pattern that analyzes an extra, unrequested file
    must not pass just because every requested file was also covered -- these
    same patterns scope fixit --apply, which would mutate the extra file too."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"
    requested_source = str(Path("a.cpp").resolve())
    unrequested_source = str((tmp_path / "unrequested.cpp").resolve())

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            output_dir.mkdir(parents=True, exist_ok=True)
            metadata = {
                "tools": [
                    {
                        "analyzers": {
                            "clang-tidy": {
                                "analyzer_statistics": {
                                    "successful": 2,
                                    "failed": 0,
                                    "successful_sources": [
                                        requested_source,
                                        unrequested_source,
                                    ],
                                    "failed_sources": [],
                                }
                            }
                        }
                    }
                ]
            }
            (output_dir / "metadata.json").write_text(json.dumps(metadata))
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["a.cpp"], _args())

    assert result == 1
    stderr = capsys.readouterr().err
    assert unrequested_source in stderr
    assert requested_source not in stderr


def test_run_codechecker_zephyr_fails_on_fixit_failure(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A failed `fixit --apply` must not be silently discarded."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
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


def test_run_codechecker_zephyr_scopes_fixit_to_file_patterns(
    tmp_path: Path, explicit_compile_commands: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Fixit --apply must be scoped to the same --file patterns as analyze, not run
    unscoped against the whole compile database."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"
    source_file = "file.cpp"
    expected_pattern = f"*{clang_tidy_script.os.path.relpath(source_file, clang_tidy_script.root_path)}"
    fixit_cmds = []

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
        elif cmd[1] == "fixit":
            fixit_cmds.append(cmd)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr([source_file], _args(fix=True))

    assert result == 0
    assert len(fixit_cmds) == 1
    fixit_cmd = fixit_cmds[0]
    assert "--file" in fixit_cmd
    assert fixit_cmd[fixit_cmd.index("--file") + 1 :] == [expected_pattern]


def test_run_codechecker_zephyr_does_not_cache_on_mismatch(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Cache must only be marked fresh once validated."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(
        work_dir: Path, platformio_ini: Path, source_files: list[str]
    ) -> Path:
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

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
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

    def fake_generate(
        work_dir: Path, platformio_ini: Path, source_files: list[str]
    ) -> Path:
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

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 0
    assert len(update_cache_calls) == 1


@pytest.fixture
def reused_cache(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    """A validated cache from a prior run: compile_commands.json and its hash file already exist, is_cached() -> True."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"
    cc_path.parent.mkdir(parents=True, exist_ok=True)
    cc_path.write_text("[]")
    cache_key_path = work_dir / ".compile_commands.hash"
    cache_key_path.write_text("stale-hash")

    monkeypatch.setattr(
        "esphome.components.nrf52.clang_tidy.generate_compile_commands",
        MagicMock(side_effect=AssertionError("must not regenerate a reused cache")),
    )
    monkeypatch.setattr(clang_tidy_hash, "is_cached", lambda *a, **k: True)
    return cache_key_path


def test_run_codechecker_zephyr_invalidates_reused_cache_on_failed_files(
    tmp_path: Path, reused_cache: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=0, failed=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert not reused_cache.exists()


def test_run_codechecker_zephyr_invalidates_reused_cache_on_metadata_error(
    tmp_path: Path, reused_cache: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        # metadata.json is never written -- simulates a CodeChecker crash.
        return MagicMock(returncode=1)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert not reused_cache.exists()


def test_run_codechecker_zephyr_reports_metadata_schema_drift_distinctly(
    tmp_path: Path,
    reused_cache: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A metadata.json that parses but no longer has the expected keys (e.g. a
    future CodeChecker minor renaming successful_sources/failed_sources) must
    be reported as a schema mismatch, not misattributed to an analyze failure."""
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            output_dir.mkdir(parents=True, exist_ok=True)
            metadata = {
                "tools": [
                    {
                        "analyzers": {
                            "clang-tidy": {
                                "analyzer_statistics": {
                                    "failed": 0,
                                    # successful_sources/failed_sources renamed --
                                    # simulates schema drift in a future CodeChecker.
                                }
                            }
                        }
                    }
                ]
            }
            (output_dir / "metadata.json").write_text(json.dumps(metadata))
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert not reused_cache.exists()
    err = capsys.readouterr().err
    assert "unexpected shape" in err
    assert "failed_sources" in err
    assert "exit 0" not in err


def test_run_codechecker_zephyr_invalidates_reused_cache_on_count_mismatch(
    tmp_path: Path, reused_cache: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=0, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert not reused_cache.exists()


def test_run_codechecker_zephyr_keeps_reused_cache_on_success(
    tmp_path: Path, reused_cache: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 0
    assert reused_cache.exists()


def test_run_codechecker_zephyr_does_not_cache_regenerated_failure(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A regenerated database that produced analyzer failures must not be cached as good."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(
        work_dir: Path, platformio_ini: Path, source_files: list[str]
    ) -> Path:
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

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            # All requested files covered, but one failed -- must not cache.
            _write_metadata(output_dir, successful=0, failed=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert update_cache_calls == []


def test_run_codechecker_zephyr_does_not_cache_on_nonzero_analyze_returncode(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """`analyze` exiting non-zero for a reason not reflected in metadata.json
    (0 failed, fully covered) must still block the cache write."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(
        work_dir: Path, platformio_ini: Path, source_files: list[str]
    ) -> Path:
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

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=1, failed=0)
            return MagicMock(returncode=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1
    assert update_cache_calls == []


def test_run_codechecker_zephyr_regenerated_failure_does_not_crash(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """regenerated=True means no cache file exists yet; invalidation on failure must be a no-op, not an error."""
    work_dir = tmp_path / "zephyr-nrf52-adafruit"
    cc_path = work_dir / "build" / "compile_commands.json"

    def fake_generate(
        work_dir: Path, platformio_ini: Path, source_files: list[str]
    ) -> Path:
        cc_path.parent.mkdir(parents=True, exist_ok=True)
        cc_path.write_text("[]")
        return cc_path

    monkeypatch.setattr(
        "esphome.components.nrf52.clang_tidy.generate_compile_commands",
        fake_generate,
    )
    monkeypatch.setattr(clang_tidy_hash, "is_cached", lambda *a, **k: False)

    output_dir = tmp_path / "codechecker-nrf52-adafruit"

    def fake_run(cmd: list[str], **kwargs: object) -> MagicMock:
        if cmd[1] == "analyze":
            _write_metadata(output_dir, successful=0, failed=1)
        return MagicMock(returncode=0)

    monkeypatch.setattr(clang_tidy_script.subprocess, "run", fake_run)
    result = clang_tidy_script.run_codechecker_zephyr(["file.cpp"], _args())

    assert result == 1


def _codechecker_matcher_regex() -> re.Pattern[str]:
    matcher_path = (
        Path(__file__).parent
        / ".."
        / ".."
        / ".github"
        / "workflows"
        / "matchers"
        / "clang-tidy-codechecker.json"
    )
    pattern = json.loads(matcher_path.read_text())["problemMatcher"][0]["pattern"][0]
    return re.compile(pattern["regexp"])


def test_codechecker_matcher_parses_report_line() -> None:
    """The sample line is generated by the installed codechecker_report_converter's
    own plaintext.format_report(), so an upstream format change fails this test
    instead of silently making the matcher emit zero annotations in CI."""
    try:
        from codechecker_report_converter.report import File, Report
        from codechecker_report_converter.report.output.plaintext import format_report
    except ImportError:
        pytest.skip("codechecker_report_converter is not installed")

    report = Report(
        file=File("/work/esphome/components/nrf52/nrf52.cpp"),
        line=42,
        column=7,
        message="use of uninitialized value 'x'",
        checker_name="clang-analyzer-core.uninitialized.Assign",
        severity="HIGH",
    )
    line = format_report(report, content_is_not_changed=True)

    regex = _codechecker_matcher_regex()
    match = regex.match(line)

    assert match is not None
    assert match.group(1) == "/work/esphome/components/nrf52/nrf52.cpp"
    assert match.group(2) == "42"
    assert match.group(3) == "7"
    assert match.group(4) == "use of uninitialized value 'x'"
    assert match.group(5) == "clang-analyzer-core.uninitialized.Assign"


@pytest.mark.parametrize(
    "line",
    [
        "Found 1 defect(s) in nrf52.cpp",
        "Found no defects in nrf52.cpp",
        "        ^",
        "    x = uninitialized_thing();",
    ],
)
def test_codechecker_matcher_ignores_non_report_lines(line: str) -> None:
    """`CodeChecker parse` also prints source excerpts and per-file summaries;
    the matcher must not misparse those into bogus annotations."""
    regex = _codechecker_matcher_regex()

    assert regex.match(line) is None
