"""A format text is formatted into a stack buffer, not a heap std::string."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.__main__ import generate_cpp_contents
from esphome.config import read_config
from esphome.core import CORE


@pytest.fixture(scope="module")
def main_cpp(request: pytest.FixtureRequest) -> str:
    config_path = Path(request.fspath).parent / "config" / "format_text_test.yaml"
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        CORE.config = read_config({})
        generate_cpp_contents(CORE.config)
        return CORE.cpp_global_section + CORE.cpp_main_section
    finally:
        CORE.config_path = original_path
        CORE.reset()


def test_action_text_uses_a_stack_buffer(main_cpp: str) -> None:
    decl = main_cpp.index("char text_buf_1[46];")
    fmt = main_cpp.index(
        'snprintf(text_buf_1, sizeof(text_buf_1), "%.1f\\302\\260C", temp->state);',
        decl,
    )
    main_cpp.index("lv_label_set_text(temp_label, text_buf_1);", fmt)
    assert 'str_sprintf("%.1f' not in main_cpp


def test_setup_text_stays_on_the_heap(main_cpp: str) -> None:
    # Setup is one long function, so a buffer there would hold its stack throughout
    # Only the two action texts get a buffer
    assert main_cpp.count("char text_buf_") == 2
    assert (
        'lv_label_set_text(temp_label, str_sprintf("%5.2f", id(temp).state).c_str());'
        in main_cpp
    )


def test_if_nan_picks_the_buffer_or_the_nan_text(main_cpp: str) -> None:
    decl = main_cpp.index("char text_buf_2[42];")
    main_cpp.index(
        'lv_label_set_text(nan_label, (std::isfinite(temp->state) ? text_buf_2 : "--"));',
        decl,
    )


def test_unbounded_format_stays_on_the_heap(main_cpp: str) -> None:
    assert (
        'lv_label_set_text(str_label, str_sprintf("name %s", "x").c_str());' in main_cpp
    )
