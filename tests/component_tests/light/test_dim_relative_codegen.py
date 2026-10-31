"""Tests for the code generated for light.dim_relative and light.effect.next/previous."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("dim_relative_actions.yaml"))


def test_dim_relative_without_transition_or_limits(main_cpp: str) -> None:
    """No transition length and no limits: the helper's default limits apply."""
    assert (
        "static void esphome__applyaction_id__fn() {\n"
        "  auto esphome__call = ::dim_light->make_call();\n"
        "  if (light::dim_relative(::dim_light, esphome__call, 0.1f)) {\n"
        "    esphome__call.perform();\n"
        "  }\n"
        "}\n"
    ) in main_cpp


def test_dim_relative_with_transition_and_limits(main_cpp: str) -> None:
    """The transition length is only set once the limits allow the call."""
    assert (
        "static void esphome__applyaction_id_2__fn() {\n"
        "  auto esphome__call = ::dim_light->make_call();\n"
        "  if (light::dim_relative(::dim_light, esphome__call, -0.05f, 0.2f, 0.9f, "
        "light::LimitMode::DO_NOTHING)) {\n"
        "    esphome__call.set_transition_length(250);\n"
        "    esphome__call.perform();\n"
        "  }\n"
        "}\n"
    ) in main_cpp


@pytest.mark.parametrize(
    ("return_type", "body"),
    [("float", "step *= 0.5f; return step;"), ("uint32_t", "return step *= 100.0f;")],
)
def test_dim_relative_lambdas_take_args_by_value(
    main_cpp: str, return_type: str, body: str
) -> None:
    """The user's lambdas get the script parameter by value, so they may modify it.

    They are called rather than reduced, so even a single return modifies only its own copy.
    """
    assert (
        "static void esphome__applyaction_id_3__fn(const std::remove_cvref_t<float> & step) {"
        in main_cpp
    )
    head = f"[](float step) -> {return_type} {{\n"
    start = main_cpp.index(head)
    assert f"      {body}\n" in main_cpp[start : main_cpp.index("}(step)", start)]


def test_effect_cycle(main_cpp: str) -> None:
    """Both actions call the shared helper, merged into one function."""
    assert (
        "  {\n"
        "  light::cycle_effect(::dim_light, true, false);\n"
        "  }\n"
        "  if (!esphome__running)\n"
        "    return;\n"
        "  {\n"
        "  light::cycle_effect(::dim_light, false, true);\n"
        "  }\n"
    ) in main_cpp


def test_dim_relative_limit_does_not_skip_merged_actions(main_cpp: str) -> None:
    """The limit check is an if block, so an action merged after it still runs."""
    fn = re.search(
        r"Automation<>\(dim_skip_then_dim\);\nnew\(\w+\) ApplyRunAction<(\w+)>",
        main_cpp,
    )
    assert fn is not None
    start = main_cpp.index(
        f"static void {fn.group(1)}(const int & esphome__running) {{"
    )
    body = main_cpp[start : main_cpp.index("\n}\n", start)]
    assert body.count("light::dim_relative(") == 2
    # The only return is the stop check between the two actions
    assert body.count("return;") == 1
