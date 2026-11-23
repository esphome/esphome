"""Tests for the code generated for light.dim_relative and light.effect.next/previous."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE
from tests.component_tests.helpers import static_function_body


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("dim_relative_actions.yaml"))


def test_dim_relative_without_transition_or_limits(main_cpp: str) -> None:
    """No transition length and no limits: the helper's default limits apply."""
    assert (
        "  auto esphome__call = ::dim_light->make_call();\n"
        "  if (light::dim_relative(::dim_light, esphome__call, 0.1f)) {\n"
        "    esphome__call.perform();\n"
        "  }\n"
    ) in static_function_body(main_cpp, "esphome__dim_plain__fn")


def test_dim_relative_with_transition_and_limits(main_cpp: str) -> None:
    """The transition length is only set once the limits allow the call."""
    assert (
        "  auto esphome__call = ::dim_light->make_call();\n"
        "  if (light::dim_relative(::dim_light, esphome__call, -0.05f, 0.2f, 0.9f, "
        "light::LimitMode::DO_NOTHING)) {\n"
        "    esphome__call.set_transition_length(250);\n"
        "    esphome__call.perform();\n"
        "  }\n"
    ) in static_function_body(main_cpp, "esphome__dim_limits__fn")


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
    fn = static_function_body(main_cpp, "esphome__dim_lambdas__fn")
    start = fn.index(f"[](float step) -> {return_type} {{\n")
    assert f"      {body}\n" in fn[start : fn.index("}(step)", start)]


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
    body = static_function_body(main_cpp, "esphome__dim_skip_then_dim__fn")
    assert body.count("light::dim_relative(") == 2
    # The only return is the stop check between the two actions
    assert body.count("return;") == 1


@pytest.mark.parametrize("action", ["light.effect.next", "light.effect.previous"])
def test_effect_cycle_rejects_light_without_effects(
    action: str, tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """Cycling a light that has no effects is refused at validation, so it never runs."""
    config = tmp_path / "no_effects.yaml"
    config.write_text(
        "esphome:\n  name: test-no-effects\n"
        "esp32:\n  board: esp32dev\n"
        "output:\n  - platform: ledc\n    id: out_a\n    pin: GPIO4\n"
        "light:\n  - platform: monochromatic\n    id: bare_light\n    output: out_a\n"
        f"script:\n  - id: cycle\n    then:\n      - {action}: bare_light\n"
    )
    CORE.config_path = config
    assert read_config({}) is None
    assert "has no effects configured" in capsys.readouterr().out
