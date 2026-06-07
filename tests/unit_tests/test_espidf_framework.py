"""Tests for esphome.espidf.framework helpers."""

# pylint: disable=protected-access

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.espidf.framework import _clone_idf_with_submodules, _parse_git_source


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        # github:// shorthand
        (
            "github://espressif/esp-idf",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "github://espressif/esp-idf@release/v6.0",
            ("https://github.com/espressif/esp-idf.git", "release/v6.0"),
        ),
        # explicit https://github.com/...git URL
        (
            "https://github.com/espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "https://github.com/espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
        (
            "https://github.com/espressif/esp-idf.git@v6.0.1",
            ("https://github.com/espressif/esp-idf.git", "v6.0.1"),
        ),
        # Tolerate a trailing ".git" on the shorthand so the user doesn't
        # silently end up with a doubled "...esp-idf.git.git" URL.
        (
            "github://espressif/esp-idf.git",
            ("https://github.com/espressif/esp-idf.git", None),
        ),
        (
            "github://espressif/esp-idf.git@master",
            ("https://github.com/espressif/esp-idf.git", "master"),
        ),
    ],
)
def test_parse_git_source_recognized(
    source: str, expected: tuple[str, str | None]
) -> None:
    assert _parse_git_source(source) == expected


@pytest.mark.parametrize(
    "source",
    [
        # archive URLs fall through to the existing download path
        "https://github.com/espressif/esp-idf/archive/refs/heads/master.zip",
        "https://dl.espressif.com/dl/esp-idf/v6.0.1/esp-idf-v6.0.1.zip",
        "https://github.com/esphome-libs/esp-idf/releases/download/v5.5.4/esp-idf-v5.5.4.tar.xz",
        # SSH and other git protocols are intentionally rejected — match
        # external_components, which only recognizes github:// + structured
        # dicts for these.
        "git@github.com:espressif/esp-idf.git",
        "ssh://git@github.com/espressif/esp-idf.git",
        "git://github.com/espressif/esp-idf.git",
        # non-GitHub .git URLs are intentionally rejected for the same reason
        "https://gitlab.com/foo/bar.git",
        "https://github.example.com/foo/bar.git",
    ],
)
def test_parse_git_source_rejected(source: str) -> None:
    assert _parse_git_source(source) is None


def _make_idf_tree(framework_path: Path) -> None:
    """Create the minimum tree _clone_idf_with_submodules sanity-checks for."""
    (framework_path / "tools").mkdir(parents=True)
    (framework_path / "tools" / "idf_tools.py").write_text("# stub\n")


def test_clone_idf_with_submodules_without_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path, "https://github.com/espressif/esp-idf.git", None
        )

    # No ref -> just clone + submodule update, no fetch/reset.
    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    assert calls[0] == [
        "git",
        "clone",
        "--depth=1",
        "--",
        "https://github.com/espressif/esp-idf.git",
        str(framework_path),
    ]
    assert calls[-1][:5] == ["git", "submodule", "update", "--init", "--recursive"]
    assert not any(c[1] == "fetch" for c in calls)
    assert not any(c[1] == "reset" for c in calls)


def test_clone_idf_with_submodules_with_ref(tmp_path: Path) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    _make_idf_tree(framework_path)

    with patch("esphome.git.run_git_command", return_value="") as run_git_command_mock:
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            "master",
        )

    calls = [c.args[0] for c in run_git_command_mock.call_args_list]
    # clone, fetch ref, reset hard, submodule update
    assert calls[0][:2] == ["git", "clone"]
    assert calls[1] == [
        "git",
        "fetch",
        "--depth=1",
        "--",
        "origin",
        "master",
    ]
    assert calls[2] == ["git", "reset", "--hard", "FETCH_HEAD"]
    assert calls[3][:5] == ["git", "submodule", "update", "--init", "--recursive"]


def test_clone_idf_with_submodules_raises_when_tree_missing(
    tmp_path: Path,
) -> None:
    framework_path = tmp_path / "idf"
    framework_path.mkdir()
    # Deliberately do NOT call _make_idf_tree — simulate a clone that
    # returned 0 but produced no tools/idf_tools.py.

    with (
        patch("esphome.git.run_git_command", return_value=""),
        pytest.raises(RuntimeError, match="no usable ESP-IDF tree"),
    ):
        _clone_idf_with_submodules(
            framework_path,
            "https://github.com/espressif/esp-idf.git",
            None,
        )
