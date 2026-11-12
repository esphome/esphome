"""Tests for esphome.components.zephyr.framework_helpers."""

from pathlib import Path
import sys
from unittest.mock import patch

import pytest

from esphome.components.zephyr import framework_helpers
from esphome.components.zephyr.framework_helpers import (
    install_sitecustomize,
    needs_venv_rebuild,
)


def test_needs_venv_rebuild_gates(tmp_path: Path) -> None:
    """The shared penv gate rebuilds on any missing or stale piece."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    good_hash = "abc123"

    # Nothing in place yet
    assert needs_venv_rebuild(python, sentinel, good_hash)

    python.write_text("")
    # Interpreter present but no sentinel
    assert needs_venv_rebuild(python, sentinel, good_hash)

    sentinel.write_text(good_hash, encoding="utf-8")
    # Everything in place
    assert not needs_venv_rebuild(python, sentinel, good_hash)

    # Stale requirements hash
    assert needs_venv_rebuild(python, sentinel, "otherhash")


@pytest.mark.skipif(
    sys.platform == "win32", reason="symlink creation needs privileges on Windows"
)
def test_needs_venv_rebuild_on_dangling_interpreter_symlink(tmp_path: Path) -> None:
    """A cached venv restored after a host interpreter upgrade has a
    bin/python symlink whose target is gone; the valid sentinel must not
    mask it."""
    penv = tmp_path / "penv"
    penv.mkdir()
    python = penv / "python"
    sentinel = penv / ".ready"
    sentinel.write_text("abc123", encoding="utf-8")
    python.symlink_to(tmp_path / "hostedtoolcache" / "3.12.14" / "python3")
    assert python.is_symlink()
    assert not python.exists()

    assert needs_venv_rebuild(python, sentinel, "abc123")


def test_install_sitecustomize_writes_patch_on_windows(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "nt"):
        install_sitecustomize(tmp_path)

    written = tmp_path / "Lib" / "site-packages" / "sitecustomize.py"
    assert written.read_text(encoding="utf-8") == framework_helpers._SITECUSTOMIZE


def test_install_sitecustomize_does_nothing_elsewhere(tmp_path: Path) -> None:
    with patch.object(framework_helpers.os, "name", "posix"):
        install_sitecustomize(tmp_path)

    assert not any(tmp_path.iterdir())
