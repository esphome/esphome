"""Tests for esphome.build_helpers.ninja."""

from __future__ import annotations

import os
from pathlib import Path
import sys
from unittest.mock import MagicMock, patch

import pytest

from esphome.build_helpers import ninja as ninja_helper
from esphome.core import EsphomeError


def test_find_ninja_prefers_path(tmp_path: Path) -> None:
    with patch("shutil.which", return_value=str(tmp_path / "ninja")):
        assert ninja_helper.find_ninja() == tmp_path / "ninja"


def test_find_ninja_falls_back_to_wheel(tmp_path: Path) -> None:
    """Without a PATH entry, the ninja PyPI wheel's binary is used."""
    binary_name = "ninja.exe" if os.name == "nt" else "ninja"
    (tmp_path / binary_name).touch()
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
    ):
        assert ninja_helper.find_ninja() == tmp_path / binary_name


def test_find_ninja_package_not_installed() -> None:
    """A missing ninja package raises the actionable message, not ImportError."""
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": None}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        ninja_helper.find_ninja()


def test_find_ninja_missing_everywhere(tmp_path: Path) -> None:
    wheel = MagicMock(BIN_DIR=str(tmp_path))
    with (
        patch("shutil.which", return_value=None),
        patch.dict(sys.modules, {"ninja": wheel}),
        pytest.raises(EsphomeError, match="ninja not found"),
    ):
        ninja_helper.find_ninja()
