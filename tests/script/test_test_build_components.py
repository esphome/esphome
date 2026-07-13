"""Unit tests for script/test_build_components.py logging helpers."""

from pathlib import Path
import sys

import pytest

# Add the script directory to the path so we can import the module under test.
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "script"))

import test_build_components as tbc  # noqa: E402


class _FakeCompleted:
    """Minimal stand-in for subprocess.CompletedProcess."""

    def __init__(self, returncode: int) -> None:
        self.returncode = returncode


@pytest.fixture
def _no_ci(monkeypatch: pytest.MonkeyPatch) -> None:
    """Ensure GITHUB_ACTIONS is unset so group markers are suppressed."""
    monkeypatch.delenv("GITHUB_ACTIONS", raising=False)


@pytest.fixture
def _ci(monkeypatch: pytest.MonkeyPatch) -> None:
    """Pretend we are running inside GitHub Actions."""
    monkeypatch.setenv("GITHUB_ACTIONS", "true")


def test_start_log_group_outside_ci_is_silent(
    _no_ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.start_log_group("hello")
    assert capsys.readouterr().out == ""


def test_end_log_group_outside_ci_is_silent(
    _no_ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.end_log_group()
    assert capsys.readouterr().out == ""


def test_start_log_group_in_ci_emits_marker(
    _ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.start_log_group("hello")
    assert capsys.readouterr().out == "::group::hello\n"


def test_end_log_group_in_ci_emits_marker(
    _ci: None, capsys: pytest.CaptureFixture[str]
) -> None:
    tbc.end_log_group()
    assert capsys.readouterr().out == "::endgroup::\n"


def _make_base_file(tmp_path: Path) -> Path:
    base_file = tmp_path / "base.yaml"
    base_file.write_text("esphome:\n  name: $component_test_file\n")
    return base_file


def test_run_esphome_test_wraps_output_in_group(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A passing single-component test is bracketed by group markers."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(0))
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / "test.esp32-idf.yaml"

    result = tbc.run_esphome_test(
        component="foo",
        test_file=test_file,
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        esphome_command="config",
        continue_on_fail=True,
    )

    out = capsys.readouterr().out
    assert result.success is True
    assert "::group::[foo] [test] [esp32-idf]" in out
    assert "::endgroup::" in out
    # The header line is printed inside the group.
    assert out.index("::group::") < out.index("> [foo]") < out.index("::endgroup::")


def test_run_esphome_test_closes_group_before_failure_report(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """On failure the group closes before the reproduce command is printed."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(1))
    repo_root = Path(tbc.__file__).parent.parent
    test_file = repo_root / "tests" / "components" / "foo" / "test.esp32-idf.yaml"

    result = tbc.run_esphome_test(
        component="foo",
        test_file=test_file,
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        esphome_command="config",
        continue_on_fail=True,
    )

    out = capsys.readouterr().out
    assert result.success is False
    assert "::endgroup::" in out


def test_run_grouped_test_wraps_output_in_group(
    _ci: None,
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A grouped test is bracketed by group markers listing its components."""
    monkeypatch.setattr(tbc.subprocess, "run", lambda *a, **k: _FakeCompleted(0))
    monkeypatch.setattr(tbc, "merge_component_configs", lambda **k: None)

    result = tbc.run_grouped_test(
        components=["foo", "bar"],
        platform="esp32-idf",
        platform_with_version="esp32-idf",
        base_file=_make_base_file(tmp_path),
        build_dir=tmp_path,
        tests_dir=tmp_path,
        esphome_command="config",
        continue_on_fail=True,
    )

    out = capsys.readouterr().out
    assert result.success is True
    assert "::group::[GROUPED: foo, bar] [esp32-idf]" in out
    assert out.index("::group::") < out.index("> [GROUPED") < out.index("::endgroup::")
