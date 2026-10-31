"""Tests for the code generated for light.dim_relative and light.effect.next/previous."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("dim_relative_actions.yaml"))


def test_dim_relative_without_transition_or_limits(main_cpp: str) -> None:
    """No transition length and no limits: the defaults apply."""
    assert (
        "new(light_dimrelativeaction_id) light::DimRelativeAction<false>(dim_light);\n"
        "light_dimrelativeaction_id->set_relative_brightness([]() -> float {\n"
        "    return 0.1f;\n"
        "});\n"
        "new(automation_id) Automation<>(dim_plain);\n"
    ) in main_cpp


def test_dim_relative_with_transition_and_limits(main_cpp: str) -> None:
    """A transition length, brightness limits and limit mode are all set."""
    assert (
        "new(light_dimrelativeaction_id_2) light::DimRelativeAction<true>(dim_light);\n"
        "light_dimrelativeaction_id_2->set_relative_brightness([]() -> float {\n"
        "    return -0.05f;\n"
        "});\n"
        "light_dimrelativeaction_id_2->set_transition_length([]() -> uint32_t {\n"
        "    return 250;\n"
        "});\n"
        "light_dimrelativeaction_id_2->set_min_max_brightness(0.2f, 0.9f);\n"
        "light_dimrelativeaction_id_2->set_limit_mode(light::LimitMode::DO_NOTHING);\n"
    ) in main_cpp


@pytest.mark.parametrize(
    ("return_type", "body"),
    [("float", "step *= 0.5f; return step;"), ("uint32_t", "return step *= 100.0f;")],
)
def test_dim_relative_lambdas_take_args_by_value(
    main_cpp: str, return_type: str, body: str
) -> None:
    """The user's lambdas get the script parameter by value, so they may modify it."""
    head = f"[](float step) -> {return_type} {{\n"
    start = main_cpp.index(head)
    assert f"    {body}\n" in main_cpp[start : main_cpp.index("});", start)]


def test_effect_cycle(main_cpp: str) -> None:
    """The direction is a template argument and include_none is always set."""
    assert (
        "new(light_lighteffectcycleaction_id) light::LightEffectCycleAction<true>(dim_light);\n"
        "light_lighteffectcycleaction_id->set_include_none(false);\n"
        "new(light_lighteffectcycleaction_id_2) light::LightEffectCycleAction<false>(dim_light);\n"
        "light_lighteffectcycleaction_id_2->set_include_none(true);\n"
    ) in main_cpp
