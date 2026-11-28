"""Tests for the validation of light.move and the code generated for light.move and light.stop."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.light.automation import LIGHT_MOVE_ACTION_SCHEMA
from esphome.core import CORE


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path("move_stop_actions.yaml"))


@pytest.mark.parametrize(
    ("fn", "statement"),
    [
        # Defaults: 5 s for the whole range, limits 1% to 100%
        (
            "esphome__applyaction_id__fn()",
            "light::move_brightness(::move_light, true, 5000, 0.01f, 1.0f);",
        ),
        (
            "esphome__applyaction_id_2__fn()",
            "light::move_brightness(::move_light, false, 2000, 0.0f, 0.8f);",
        ),
    ],
)
def test_move(main_cpp: str, fn: str, statement: str) -> None:
    assert f"static void {fn} {{\n  {statement}\n}}\n" in main_cpp


def test_move_lambdas_get_the_script_args(main_cpp: str) -> None:
    start = main_cpp.index(
        "static void esphome__applyaction_id_3__fn(const std::remove_cvref_t<bool> & up, "
        "const std::remove_cvref_t<int> & ms) {\n"
        "  light::move_brightness(::move_light, [](bool up, int ms) -> bool {\n"
    )
    body = main_cpp[start : main_cpp.index("\n}\n", start)]
    assert "[](bool up, int ms) -> int32_t {\n" in body
    assert body.endswith("}(up, ms), 0.01f, 1.0f);")


def test_stop(main_cpp: str) -> None:
    """Both forms call stop_move(), merged into one function."""
    assert (
        "  {\n"
        "  ::move_light->stop_move();\n"
        "  }\n"
        "  if (!esphome__running)\n"
        "    return;\n"
        "  {\n"
        "  ::stop_light->stop_move();\n"
        "  }\n"
    ) in main_cpp


def test_define_only_when_used(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("move_stop_actions.yaml"))
    assert any(d.name == "USE_LIGHT_MOVE" for d in CORE.defines)
    CORE.reset()
    generate_main(component_config_path("dim_relative_actions.yaml"))
    assert not any(d.name == "USE_LIGHT_MOVE" for d in CORE.defines)


@pytest.mark.parametrize(
    ("config", "match"),
    [
        (
            {"brightness_limits": {"min_brightness": "50%", "max_brightness": "50%"}},
            "must be below",
        ),
        (
            {"brightness_limits": {"min_brightness": "60%", "max_brightness": "50%"}},
            "must be below",
        ),
        ({"duration": "0s"}, "higher than 0s"),
        ({"duration": "25d"}, "must be at most"),
        ({"direction": "LEFT"}, "Unknown value 'LEFT'"),
    ],
)
def test_move_rejects(config: dict[str, Any], match: str) -> None:
    with pytest.raises(cv.Invalid, match=match):
        LIGHT_MOVE_ACTION_SCHEMA({"id": "move_light", "direction": "UP", **config})
