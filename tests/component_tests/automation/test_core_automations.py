"""Tests for on_boot, on_shutdown and on_loop codegen."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from tests.component_tests.helpers import constructed, function_body

CONFIG = "core_automations.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


def _automation_of(main_cpp: str, trigger: str) -> str:
    """The Automation built on ``trigger``, found by its trigger rather than an id counter."""
    match = re.search(rf"new\((\w+)\) Automation<>\({trigger}\);", main_cpp)
    assert match is not None, trigger
    return match.group(1)


def _registered(main_cpp: str) -> list[str]:
    return re.findall(r"App\.register_component_\((\w+), \d+\);", main_cpp)


def test_core_automations_registration_order(main_cpp: str) -> None:
    """Each block is one component, registered in config order after the core ones."""
    assert _registered(main_cpp)[-8:] == [
        "startuptrigger_id",
        "startuptrigger_id_2",
        "startuptrigger_id_3",
        "manual_boot",
        "shutdowntrigger_id",
        "shutdowntrigger_id_2",
        "looptrigger_id",
        "looptrigger_id_2",
    ]


def _delayed_automation(main_cpp: str, function: str) -> str:
    """The Automation that ``function`` triggers, checked to hold a delay; found through the
    function rather than an id counter."""
    match = re.fullmatch(
        r"\n  ::(\w+)->trigger\(\);", function_body(main_cpp, function)
    )
    assert match is not None, function
    automation = match.group(1)
    assert f"new({automation}) Automation<>();" in main_cpp
    assert re.search(rf"{automation}->add_action\(delayaction_id\w*\);", main_cpp)
    return automation


def test_on_boot_inline_priorities(main_cpp: str) -> None:
    """All-inline on_boot blocks call one function from setup() at their priority, default 600.

    The priority is the float's bit pattern: 0x44480000 is 800.0f, 0x44160000 is 600.0f.
    """
    assert constructed(main_cpp, "startuptrigger_id") == (
        "StartupCallback<esphome__startuptrigger_id__fn, 0x44480000u>()"
    )
    assert constructed(main_cpp, "startuptrigger_id_2") == (
        "StartupCallback<esphome__startuptrigger_id_2__fn, 0x44160000u>()"
    )
    assert "Automation<>(startuptrigger_id" not in main_cpp
    # Both actions of the first block run in the one function, with no stop checks
    body = function_body(main_cpp, "esphome__startuptrigger_id__fn")
    assert "boot high" in body
    assert "boot high lambda" in body
    assert "esphome__running" not in body
    assert "Automation" not in body
    assert "boot default" in function_body(main_cpp, "esphome__startuptrigger_id_2__fn")


def test_on_boot_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    assert constructed(main_cpp, "startuptrigger_id_3") == (
        "StartupCallback<esphome__startuptrigger_id_3__fn, 0xC2C80000u>()"
    )
    _delayed_automation(main_cpp, "esphome__startuptrigger_id_3__fn")


def test_on_boot_manual_trigger_id_keeps_trigger(main_cpp: str) -> None:
    """A trigger_id set by hand can be used from lambdas, so the Trigger stays."""
    assert constructed(main_cpp, "manual_boot") == "StartupTrigger(250.5f)"
    _automation_of(main_cpp, "manual_boot")


def test_on_shutdown_priorities(main_cpp: str) -> None:
    """on_shutdown calls its function at its priority; a delay keeps the Automation."""
    assert constructed(main_cpp, "shutdowntrigger_id") == (
        "ShutdownCallback<esphome__shutdowntrigger_id__fn, 0x442F0000u>()"
    )
    assert "shutdown inline" in function_body(
        main_cpp, "esphome__shutdowntrigger_id__fn"
    )
    assert constructed(main_cpp, "shutdowntrigger_id_2") == (
        "ShutdownCallback<esphome__shutdowntrigger_id_2__fn, 0x44160000u>()"
    )
    _delayed_automation(main_cpp, "esphome__shutdowntrigger_id_2__fn")


def test_on_loop(main_cpp: str) -> None:
    """on_loop calls its function each loop; a delay keeps the Automation."""
    assert constructed(main_cpp, "looptrigger_id") == (
        "LoopCallback<esphome__looptrigger_id__fn>()"
    )
    assert "loop inline" in function_body(main_cpp, "esphome__looptrigger_id__fn")
    assert "HasLoopOverride<LoopCallback<esphome__looptrigger_id__fn>>" in main_cpp
    assert constructed(main_cpp, "looptrigger_id_2") == (
        "LoopCallback<esphome__looptrigger_id_2__fn>()"
    )
    _delayed_automation(main_cpp, "esphome__looptrigger_id_2__fn")


def test_on_boot_registered_before_its_actions_are_built(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The block keeps its place among components with the same priority while it waits."""
    main_cpp = generate_main(component_config_path("on_boot_waits_for_ids.yaml"))

    assert _registered(main_cpp)[-3:] == [
        "startuptrigger_id",
        "interval_intervaltrigger_id",
        "some_sensor",
    ]
