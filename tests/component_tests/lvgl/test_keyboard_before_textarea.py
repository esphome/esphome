"""Regression test: a keyboard: declared before its textarea: sibling in the
same widgets: list must not deadlock code generation.

KeyboardType.to_code() unconditionally awaits get_widgets() for its textarea
id. add_widgets() walks a container's children with a single sequential
await, not a separate scheduled job, so if the textarea hasn't been created
yet, this task is waiting on its own later progress and can never resume.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE, EsphomeError


def test_keyboard_before_textarea_does_not_deadlock() -> None:
    config_path = (
        Path(__file__).parent / "config" / "keyboard_before_textarea_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        try:
            generate_cpp_contents(CORE.config)
        except EsphomeError as e:
            pytest.fail(f"code generation deadlocked: {e}")
    finally:
        CORE.config_path = original_path
        CORE.reset()
