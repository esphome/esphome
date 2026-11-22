"""Tests for on_boot, on_shutdown and on_loop codegen."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import actions_body, constructed, delayed_automation

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
    """All-inline on_boot blocks call one function from setup() at their priority, default 600.

    The priority is the float's bit pattern: 0x44480000 is 800.0f, 0x44160000 is 600.0f.
    """
    assert constructed(main_cpp, "startupcallback_id") == (
        "StartupCallback<esphome__startupcallback_id__fn, 0x44480000>()"
    )
    assert constructed(main_cpp, "startupcallback_id_2") == (
        "StartupCallback<esphome__startupcallback_id_2__fn, 0x44160000>()"
    )
    assert "Automation<>(startupcallback_id" not in main_cpp
    # Both actions of the first block run in the one function, with no stop checks
    body = actions_body(main_cpp, "esphome__startupcallback_id__fn")
    assert "boot high" in body
    assert "boot high lambda" in body
    assert "esphome__running" not in body
    assert "Automation" not in body
    assert "boot default" in actions_body(main_cpp, "esphome__startupcallback_id_2__fn")


def test_on_boot_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    assert constructed(main_cpp, "startupcallback_id_3") == (
        "StartupCallback<esphome__startupcallback_id_3__fn, 0xC2C80000>()"
    )
    delayed_automation(main_cpp, "esphome__startupcallback_id_3__fn")


def test_on_boot_manual_trigger_id_names_the_callback(main_cpp: str) -> None:
    """A trigger_id set by hand only names the callback component; no Trigger is built."""
    assert constructed(main_cpp, "manual_boot") == (
        "StartupCallback<esphome__manual_boot__fn, 0x437A8000>()"
    )
    assert "boot manual" in actions_body(main_cpp, "esphome__manual_boot__fn")
    assert "Automation<>(manual_boot)" not in main_cpp


def test_on_shutdown_priorities(main_cpp: str) -> None:
    """on_shutdown calls its function at its priority; a delay keeps the Automation."""
    assert constructed(main_cpp, "shutdowncallback_id") == (
        "ShutdownCallback<esphome__shutdowncallback_id__fn, 0x442F0000>()"
    )
    assert "shutdown inline" in actions_body(
        main_cpp, "esphome__shutdowncallback_id__fn"
    )
    assert constructed(main_cpp, "shutdowncallback_id_2") == (
        "ShutdownCallback<esphome__shutdowncallback_id_2__fn, 0x44160000>()"
    )
    delayed_automation(main_cpp, "esphome__shutdowncallback_id_2__fn")


def test_on_loop(main_cpp: str) -> None:
    """on_loop calls its function each loop; a delay keeps the Automation."""
    assert constructed(main_cpp, "loopcallback_id") == (
        "LoopCallback<esphome__loopcallback_id__fn>()"
    )
    assert "loop inline" in actions_body(main_cpp, "esphome__loopcallback_id__fn")
    assert "HasLoopOverride<LoopCallback<esphome__loopcallback_id__fn>>" in main_cpp
    assert constructed(main_cpp, "loopcallback_id_2") == (
        "LoopCallback<esphome__loopcallback_id_2__fn>()"
    )
    delayed_automation(main_cpp, "esphome__loopcallback_id_2__fn")


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
