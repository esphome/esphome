"""A value lambda in an lvgl action that changes a trigger arg gets its own copy of it.

The action's generated function takes the trigger args by const reference, so a lambda body
reduced inline against them would not compile.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = (
        Path(request.fspath).parent / "config" / "action_value_lambda_copy_test.yaml"
    )
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_value_lambda_takes_trigger_arg_by_value(main_cpp: str) -> None:
    assert "const std::remove_cvref_t<float> & x" in main_cpp
    assert "[](float x, " in main_cpp
    assert "return ++x;" in main_cpp
