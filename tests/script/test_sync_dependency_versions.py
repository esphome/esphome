"""Unit tests for script/sync_dependency_versions.py."""

from pathlib import Path
import subprocess
import sys

import pytest
import yamlrocks

sys.path.insert(0, str((Path(__file__).parent / ".." / ".." / "script").resolve()))

import sync_dependency_versions as sync_mod  # noqa: E402

PRECOMMIT = """\
# See https://pre-commit.com for more information
repos:
  - repo: https://github.com/astral-sh/ruff-pre-commit
    # Ruff version.
    rev: v0.1.0
    hooks:
      - id: ruff
  - repo: https://github.com/PyCQA/flake8
    rev: 7.0.0
    hooks:
      - id: flake8
  - repo: https://github.com/asottile/pyupgrade
    rev: v3.0.0
    hooks:
      - id: pyupgrade
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v13.0.1
    hooks:
      - id: clang-format
  - repo: https://github.com/adrienverge/yamllint.git
    rev: v1.0.0
    hooks:
      - id: yamllint
  - repo: local
    hooks:
      - id: pylint
"""

REQ_TEST = """\
pylint==4.0.8
flake8==7.1.0
ruff==0.2.0  # comment
pyupgrade==3.0.0
"""

REQ_DEV = """\
clang-format==13.0.1
yamllint==1.0.0
"""

RUFF_REPO = "https://github.com/astral-sh/ruff-pre-commit"
DUPLICATE_RUFF_BLOCK = f"  - repo: {RUFF_REPO}\n    rev: v0.3.0\n    hooks: []\n"

EXPECTED_DRIFT = ["ruff: 0.1.0 -> 0.2.0", "flake8: 7.0.0 -> 7.1.0"]
EXPECTED_PRECOMMIT = PRECOMMIT.replace("rev: v0.1.0", "rev: v0.2.0").replace(
    "rev: 7.0.0", "rev: 7.1.0"
)


@pytest.fixture
def root(tmp_path: Path) -> Path:
    """A fake checkout where ruff (v-prefixed) and flake8 (bare) have drifted."""
    (tmp_path / ".pre-commit-config.yaml").write_text(PRECOMMIT)
    (tmp_path / "requirements_test.txt").write_text(REQ_TEST)
    (tmp_path / "requirements_dev.txt").write_text(REQ_DEV)
    return tmp_path


def _load(text: str) -> object:
    return yamlrocks.loads(text.encode(), option=yamlrocks.OPT_ROUND_TRIP)


@pytest.mark.parametrize(
    ("requirements", "expected"),
    [
        ("prek==0.5.1  # comment\n", "0.5.1"),
        ("Prek==0.5.1\n", "0.5.1"),
        ("other==1.0\nprek==0.5.1\n", "0.5.1"),
        ("prek>=0.5.1\n", None),
        ("prek-extra==0.5.1\n", None),
        ("", None),
    ],
)
def test_read_requirement_version(requirements: str, expected: str | None) -> None:
    assert sync_mod.read_requirement_version(requirements, "prek") == expected


def test_find_repo_entry() -> None:
    entry = sync_mod.find_repo_entry(_load(PRECOMMIT), RUFF_REPO)
    assert entry["rev"] == "v0.1.0"


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("hooks: []\n", "missing key 'repos'"),
        ("repos:\n  - rev: 1.0.0\n", "missing key 'repo'"),
        (PRECOMMIT + DUPLICATE_RUFF_BLOCK, "found 2"),
        ("repos:\n  - repo: other\n    rev: 1.0.0\n", "found 0"),
    ],
)
def test_find_repo_entry_errors(text: str, message: str) -> None:
    with pytest.raises(sync_mod.SyncError, match=message):
        sync_mod.find_repo_entry(_load(text), RUFF_REPO)


@pytest.mark.parametrize(
    ("rev", "expected"),
    [("v0.1.0", ("v", "0.1.0")), ("7.0.0", ("", "7.0.0")), ("'1.0'", ("", "1.0"))],
)
def test_current_rev(rev: str, expected: tuple[str, str]) -> None:
    doc = _load(f"repos:\n  - repo: {RUFF_REPO}\n    rev: {rev}\n")
    assert sync_mod.current_rev(doc["repos"][0], RUFF_REPO) == expected


@pytest.mark.parametrize(
    ("block", "message"),
    [("    hooks: []\n", "has no rev"), ("    rev: 1.0\n", "not a string: 1.0")],
)
def test_current_rev_errors(block: str, message: str) -> None:
    doc = _load(f"repos:\n  - repo: {RUFF_REPO}\n{block}")
    with pytest.raises(sync_mod.SyncError, match=message):
        sync_mod.current_rev(doc["repos"][0], RUFF_REPO)


def test_sync_reports_without_writing(root: Path) -> None:
    assert sync_mod.sync(root, write=False) == EXPECTED_DRIFT
    assert (root / ".pre-commit-config.yaml").read_text() == PRECOMMIT


def test_sync_writes_keeps_layout_and_is_idempotent(root: Path) -> None:
    assert sync_mod.sync(root, write=True) == EXPECTED_DRIFT
    assert (root / ".pre-commit-config.yaml").read_text() == EXPECTED_PRECOMMIT
    assert sync_mod.sync(root, write=True) == []


def test_sync_does_not_touch_a_config_that_matches(root: Path) -> None:
    (root / ".pre-commit-config.yaml").write_text(EXPECTED_PRECOMMIT)
    before = (root / ".pre-commit-config.yaml").stat().st_mtime_ns
    assert sync_mod.sync(root, write=True) == []
    assert (root / ".pre-commit-config.yaml").stat().st_mtime_ns == before


def test_sync_missing_requirement_pin(root: Path) -> None:
    (root / "requirements_dev.txt").write_text("")
    with pytest.raises(sync_mod.SyncError, match="no 'clang-format==' pin"):
        sync_mod.sync(root, write=True)


def test_sync_propagates_config_errors(root: Path) -> None:
    (root / ".pre-commit-config.yaml").write_text(PRECOMMIT + DUPLICATE_RUFF_BLOCK)
    with pytest.raises(sync_mod.SyncError, match="found 2"):
        sync_mod.sync(root, write=True)


def test_main_check_reports_drift(
    root: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    assert sync_mod.main(["--check", "--root", str(root)]) == 1
    assert capsys.readouterr().out.splitlines() == EXPECTED_DRIFT
    assert (root / ".pre-commit-config.yaml").read_text() == PRECOMMIT


def test_main_writes_then_check_is_clean(
    root: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    assert sync_mod.main(["--root", str(root)]) == 0
    assert capsys.readouterr().out.splitlines() == EXPECTED_DRIFT
    assert sync_mod.main(["--check", "--root", str(root)]) == 0
    assert capsys.readouterr().out == ""


def test_main_reports_sync_error(
    root: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    (root / "requirements_dev.txt").write_text("")
    assert sync_mod.main(["--root", str(root)]) == 1
    assert (
        "error: requirements_dev.txt: no 'clang-format==' pin"
        in capsys.readouterr().err
    )


def test_main_defaults_to_repo_root(monkeypatch: pytest.MonkeyPatch) -> None:
    seen: dict[str, object] = {}

    def fake_sync(root: Path, *, write: bool) -> list[str]:
        seen["root"] = root
        seen["write"] = write
        return []

    monkeypatch.setattr(sync_mod, "sync", fake_sync)
    assert sync_mod.main([]) == 0
    assert seen == {"root": sync_mod.REPO_ROOT, "write": True}


def test_repository_is_in_sync() -> None:
    """The real checkout must match; a failure here means a rev has drifted.

    Also proves every SYNC_TARGETS entry still resolves in the real files.
    """
    assert sync_mod.sync(sync_mod.REPO_ROOT, write=False) == []


def test_cli_entry_point(root: Path) -> None:
    """Run the script the way the workflow does, as a subprocess."""
    script = Path(sync_mod.__file__)
    result = subprocess.run(
        [sys.executable, str(script), "--check", "--root", str(root)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 1
    assert result.stdout.splitlines() == EXPECTED_DRIFT
