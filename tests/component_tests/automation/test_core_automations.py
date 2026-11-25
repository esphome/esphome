"""Tests for on_boot, on_shutdown, on_loop and project on_update codegen."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import (
    callback_function,
    constructed,
    delayed_automation,
    function_body,
)

CONFIG = "core_automations.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


def _registered(main_cpp: str) -> list[str]:
    return re.findall(r"App\.register_component_\((\w+), \d+\);", main_cpp)


def test_core_automations_registration_order(main_cpp: str) -> None:
    """Each block is one component, registered in config order after the core ones."""
    assert _registered(main_cpp)[-8:] == [
        "startupcallback_id",
        "startupcallback_id_2",
        "startupcallback_id_3",
        "manual_boot",
        "shutdowncallback_id",
        "shutdowncallback_id_2",
        "loopcallback_id",
        "loopcallback_id_2",
    ]


def test_on_boot_inline_priorities(main_cpp: str) -> None:
    """All-inline on_boot blocks run their actions in one function from setup(), default 600.

    The priority is the float's bit pattern: 0x44480000 is 800.0f, 0x44160000 is 600.0f.
    """
    high = callback_function(main_cpp, "startupcallback_id")
    assert constructed(main_cpp, "startupcallback_id") == (
        f"StartupCallback<{high}, 0x44480000>()"
    )
    default = callback_function(main_cpp, "startupcallback_id_2")
    assert constructed(main_cpp, "startupcallback_id_2") == (
        f"StartupCallback<{default}, 0x44160000>()"
    )
    # Both actions of the first block run in the one function, with no stop checks
    body = function_body(main_cpp, high)
    assert "boot high" in body
    assert "boot high lambda" in body
    assert "esphome__" not in body
    assert "Automation" not in body
    assert "boot default" in function_body(main_cpp, default)


def test_on_boot_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    function = callback_function(main_cpp, "startupcallback_id_3")
    assert constructed(main_cpp, "startupcallback_id_3") == (
        f"StartupCallback<{function}, 0xC2C80000>()"
    )
    delayed_automation(main_cpp, function)


def test_on_boot_manual_trigger_id_names_the_callback(main_cpp: str) -> None:
    """A trigger_id set by hand only names the callback component; no Trigger is built."""
    function = callback_function(main_cpp, "manual_boot")
    assert constructed(main_cpp, "manual_boot") == (
        f"StartupCallback<{function}, 0x437A8000>()"
    )
    assert "boot manual" in function_body(main_cpp, function)
    assert "Automation<>(manual_boot)" not in main_cpp


def test_on_shutdown_priorities(main_cpp: str) -> None:
    """on_shutdown calls its function at its priority; a delay keeps the Automation."""
    inline = callback_function(main_cpp, "shutdowncallback_id")
    assert constructed(main_cpp, "shutdowncallback_id") == (
        f"ShutdownCallback<{inline}, 0x442F0000>()"
    )
    assert "shutdown inline" in function_body(main_cpp, inline)
    delayed = callback_function(main_cpp, "shutdowncallback_id_2")
    assert constructed(main_cpp, "shutdowncallback_id_2") == (
        f"ShutdownCallback<{delayed}, 0x44160000>()"
    )
    delayed_automation(main_cpp, delayed)


def test_on_loop(main_cpp: str) -> None:
    """on_loop calls its function each loop; a delay keeps the Automation."""
    inline = callback_function(main_cpp, "loopcallback_id")
    assert constructed(main_cpp, "loopcallback_id") == f"LoopCallback<{inline}>()"
    assert "loop inline" in function_body(main_cpp, inline)
    assert f"HasLoopOverride<LoopCallback<{inline}>>" in main_cpp
    delayed = callback_function(main_cpp, "loopcallback_id_2")
    assert constructed(main_cpp, "loopcallback_id_2") == f"LoopCallback<{delayed}>()"
    delayed_automation(main_cpp, delayed)


def test_on_boot_registered_before_its_actions_are_built(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The block keeps its place among components with the same priority while it waits."""
    main_cpp = generate_main(component_config_path("on_boot_waits_for_ids.yaml"))

    assert _registered(main_cpp)[-3:] == [
        "startupcallback_id",
        "interval_intervalcallback_id",
        "some_sensor",
    ]


def test_on_boot_function_name_skips_config_ids(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A config id that matches the generated name pushes the function to another name."""
    main_cpp = generate_main(component_config_path("on_boot_name_collision.yaml"))
    function = callback_function(main_cpp, "startupcallback_id")
    # Built from the colliding name, so the fixture does collide
    assert function.startswith("esphome__automation_id__fn")
    assert function != "esphome__automation_id__fn"
    assert f"static void {function}()" in main_cpp


def test_project_on_update(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """on_update calls its function with the previous version; a delay keeps the Automation."""
    main_cpp = generate_main(component_config_path("project_on_update.yaml"))
    inline = callback_function(main_cpp, "projectupdatecallback_id")
    assert constructed(main_cpp, "projectupdatecallback_id") == (
        f"ProjectUpdateCallback<{inline}>()"
    )
    assert (
        f"static void {inline}(const std::remove_cvref_t<std::string> & version)"
        in main_cpp
    )
    body = function_body(main_cpp, inline)
    assert "updated from %s" in body
    assert "Automation" not in body
    delayed = callback_function(main_cpp, "projectupdatecallback_id_2")
    delayed_automation(main_cpp, delayed, "version", "std::string")
