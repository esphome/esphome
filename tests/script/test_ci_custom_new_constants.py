"""Tests for the ci-custom lint that rejects a new constant defined in two or more files.

Each test builds a small git repository, then runs ci-custom's main() in it so the real
file scan, post checks and git lookups of the base commit are exercised.
"""

import importlib.util
from pathlib import Path
import subprocess
import sys
from types import ModuleType

import pytest

SCRIPT_DIR = (Path(__file__).parent / ".." / ".." / "script").resolve()
sys.path.insert(0, str(SCRIPT_DIR))


def _load_ci_custom() -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "ci_custom", SCRIPT_DIR / "ci-custom.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def ci_custom() -> ModuleType:
    # A fresh module per test, so the collected uses and errors start empty.
    return _load_ci_custom()


@pytest.fixture
def repo(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    for var in ("GITHUB_EVENT_NAME", "GITHUB_ACTIONS", "GIT_DIR", "GIT_INDEX_FILE"):
        monkeypatch.delenv(var, raising=False)
    monkeypatch.setenv("GIT_CONFIG_GLOBAL", "/dev/null")
    monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
    for var in ("AUTHOR", "COMMITTER"):
        monkeypatch.setenv(f"GIT_{var}_NAME", "Test")
        monkeypatch.setenv(f"GIT_{var}_EMAIL", "test@example.com")
    monkeypatch.chdir(tmp_path)
    _git("init", "-q", "-b", "dev")
    return tmp_path


def _git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], check=True, capture_output=True, text=True
    ).stdout.strip()


def _write(files: dict[str, str]) -> None:
    for name, content in files.items():
        path = Path(name)
        path.parent.mkdir(parents=True, exist_ok=True)
        # newline="\n" keeps Windows from writing CRLF, which other lints reject.
        path.write_text(content, encoding="utf-8", newline="\n")
    _git("add", "-A")


def _commit(files: dict[str, str]) -> str:
    _write(files)
    _git("commit", "-q", "--allow-empty", "-m", "commit")
    return _git("rev-parse", "HEAD")


def _run(
    ci_custom: ModuleType,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> tuple[int, str]:
    monkeypatch.setattr(sys, "argv", ["ci-custom.py"])
    result = ci_custom.main()
    out = capsys.readouterr()
    return result, out.out + out.err


def _on_branch_from_dev(base_files: dict[str, str]) -> None:
    """Commit base_files as dev, publish it as origin/dev, and start a feature branch."""
    base = _commit(base_files)
    _git("update-ref", "refs/remotes/origin/dev", base)
    _git("checkout", "-q", "-b", "feature")


A = "esphome/components/a/sensor.py"
B = "esphome/components/b/sensor.py"


@pytest.mark.parametrize("annotation", ["", ": str"])
@pytest.mark.parametrize("name", ["CONF_FOO_BAR", "UNIT_FOO_BAR"])
def test_new_constant_in_two_files_fails(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
    name: str,
    annotation: str,
) -> None:
    _on_branch_from_dev(
        {"esphome/components/other/sensor.py": 'CONF_ZZ_OTHER = "zz_other"\n'}
    )
    definition = f'{name}{annotation} = "foo_bar"\n'
    _write({A: definition, B: definition})

    result, out = _run(ci_custom, monkeypatch, capsys)

    assert result == 1
    assert f"New constant {name} is defined in 2 files" in out
    assert f"({Path(A)}, {Path(B)})" in out
    assert "esphome/const.py if core code uses it" in out
    assert "esphome/components/const/__init__.py" in out


def test_new_constant_in_one_file_passes(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    _on_branch_from_dev({})
    _write({A: 'CONF_FOO_BAR = "foo_bar"\n'})

    assert _run(ci_custom, monkeypatch, capsys) == (0, "")


@pytest.mark.parametrize("annotation", ["", ": str"])
def test_constant_already_on_dev_added_in_second_file_passes(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
    annotation: str,
) -> None:
    _on_branch_from_dev({A: f'UNIT_FOO_BAR{annotation} = "fb"\n'})
    _write({B: 'UNIT_FOO_BAR = "fb"\n'})

    assert _run(ci_custom, monkeypatch, capsys) == (0, "")


def test_files_outside_esphome_are_not_counted(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    _on_branch_from_dev({})
    _write(
        {A: 'CONF_FOO_BAR = "foo_bar"\n', "tests/x.py": 'CONF_FOO_BAR = "foo_bar"\n'}
    )

    assert _run(ci_custom, monkeypatch, capsys) == (0, "")


def test_without_a_dev_remote_the_check_is_skipped(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    _commit({})
    _git("remote", "add", "fork", "https://example.invalid/fork.git")
    _write({A: 'CONF_FOO_BAR = "foo_bar"\n', B: 'CONF_FOO_BAR = "foo_bar"\n'})

    assert _run(ci_custom, monkeypatch, capsys) == (0, "")


def test_pull_request_compares_against_the_merge_commit_parent(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    # No dev remote ref here: a pull request run must use HEAD^1, as GitHub checks out
    # a merge commit whose first parent is the target branch.
    _commit({A: 'CONF_ZZ_OLD = "zz_old"\n'})
    _git("checkout", "-q", "-b", "feature")
    _commit(
        {
            B: 'CONF_ZZ_OLD = "zz_old"\nCONF_ZZ_NEW = "zz_new"\n',
            A: 'CONF_ZZ_OLD = "zz_old"\nCONF_ZZ_NEW = "zz_new"\n',
        }
    )
    _git("checkout", "-q", "dev")
    _commit({"esphome/components/c/sensor.py": "\n"})
    _git("merge", "-q", "--no-ff", "-m", "merge", "feature")
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    result, out = _run(ci_custom, monkeypatch, capsys)

    assert result == 1
    assert "New constant CONF_ZZ_NEW " in out
    assert "CONF_ZZ_OLD" not in out


def test_pull_request_without_parent_commit_warns_and_skips(
    ci_custom: ModuleType,
    repo: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture,
) -> None:
    _commit({A: 'CONF_FOO_BAR = "foo_bar"\n', B: 'CONF_FOO_BAR = "foo_bar"\n'})
    monkeypatch.setenv("GITHUB_EVENT_NAME", "pull_request")

    result, out = _run(ci_custom, monkeypatch, capsys)

    assert result == 0
    assert "fetch-depth: 2" in out
