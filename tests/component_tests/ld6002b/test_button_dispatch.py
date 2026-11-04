"""Code generation tests for the ld6002b button dispatcher.

Each button block gets one function that maps the pressed entity to its button
type and calls its own hub; the last button of a block is the fallback type.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

YAML = Path(__file__).parent / "button_dispatch.yaml"


def _fn_body(main_cpp: str, fn: str) -> str:
    match = re.search(
        rf"static void {fn}\(button::Button \* entity\) \{{\n(.*?)\n\}}",
        main_cpp,
        re.DOTALL,
    )
    assert match is not None, f"{fn} not defined"
    return match.group(1)


@pytest.mark.parametrize(
    ("fn", "type_expr", "hub"),
    [
        # Two blocks on hub_a: each named after its first button, chain ends in the fallback.
        # The wake button is the model's own, so this block casts every type to uint8_t.
        (
            "esphome__a_get_delay__button_fn",
            (
                "entity == ::a_get_delay ? static_cast<uint8_t>(ld600x::BUTTON_GET_DELAY) : "
                "entity == ::a_get_sensitivity ? static_cast<uint8_t>(ld600x::BUTTON_GET_SENSITIVITY) : "
                "static_cast<uint8_t>(ld6002b::BUTTON_WAKE)"
            ),
            "hub_a",
        ),
        (
            "esphome__a_get_z_range__button_fn",
            "ld600x::BUTTON_GET_Z_RANGE",
            "hub_a",
        ),
        # A single button on a second hub: no chain, its own hub.
        (
            "esphome__b_get_installation__button_fn",
            "ld600x::BUTTON_GET_INSTALLATION",
            "hub_b",
        ),
    ],
)
def test_dispatch_function(
    generate_main: Callable[[str | Path], str], fn: str, type_expr: str, hub: str
) -> None:
    main_cpp = generate_main(YAML)

    assert main_cpp.count(f"static void {fn}(button::Button * entity) {{") == 1
    assert _fn_body(main_cpp, fn) == (
        f"  const auto type = {type_expr};\n  ::{hub}->press_button(type);"
    )


def test_each_button_uses_its_block_function(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(YAML)

    expected = {
        "a_get_delay": "esphome__a_get_delay__button_fn",
        "a_get_sensitivity": "esphome__a_get_delay__button_fn",
        "a_wake": "esphome__a_get_delay__button_fn",
        "a_get_z_range": "esphome__a_get_z_range__button_fn",
        "b_get_installation": "esphome__b_get_installation__button_fn",
    }
    for button_id, fn in expected.items():
        assert f"new({button_id}) button::ApplyButton<{fn}>();" in main_cpp
    # One definition per block, no more.
    assert (
        len(re.findall(r"static void esphome__\w+__button_fn\(.*\) \{", main_cpp)) == 3
    )
