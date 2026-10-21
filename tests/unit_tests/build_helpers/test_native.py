"""Tests for esphome.build_helpers.native."""

from __future__ import annotations

import logging

import pytest

from esphome.build_helpers import native
from esphome.const import Toolchain
from esphome.core import CORE


def test_warn_ignored_platformio_options(caplog: pytest.LogCaptureFixture) -> None:
    """Options a native build drops are warned by name; consumed ones stay quiet."""
    CORE.toolchain = Toolchain.ARDUINO
    CORE.platformio_options = {
        "lib_ignore": ["x"],
        "board_build.filesystem": "littlefs",
    }
    with caplog.at_level(logging.WARNING):
        native.warn_ignored_platformio_options({"lib_ignore"})
    assert "platformio_options->board_build.filesystem is ignored" in caplog.text
    assert "native 'arduino' toolchain" in caplog.text
    assert "lib_ignore" not in caplog.text


def test_warn_ignored_platformio_options_without_options(
    caplog: pytest.LogCaptureFixture,
) -> None:
    CORE.platformio_options = None
    with caplog.at_level(logging.WARNING):
        native.warn_ignored_platformio_options(())
    assert caplog.text == ""
