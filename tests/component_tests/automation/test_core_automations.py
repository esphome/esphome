"""Tests for on_boot, on_shutdown and on_loop codegen."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

CONFIG = "core_automations.yaml"


@pytest.fixture
def main_cpp(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> str:
    return generate_main(component_config_path(CONFIG))


def _constructed(main_cpp: str, var: str) -> str:
    match = re.search(rf"new\({var}\) ([^;]+);", main_cpp)
    assert match is not None, var
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


def _function_body(main_cpp: str, name: str) -> str:
    match = re.search(rf"static void {name}\(\) \{{(.*?)\n\}}\n", main_cpp, re.DOTALL)
    assert match is not None, name
    return match.group(1)


def test_on_boot_inline_priorities(main_cpp: str) -> None:
    """All-inline on_boot blocks call one function from setup() at their priority, default 600.

    The priority is the float's bit pattern: 0x44480000 is 800.0f, 0x44160000 is 600.0f.
    """
    assert _constructed(main_cpp, "startuptrigger_id") == (
        "StartupCallback<esphome__startuptrigger_id__fn, 0x44480000u>()"
    )
    assert _constructed(main_cpp, "startuptrigger_id_2") == (
        "StartupCallback<esphome__startuptrigger_id_2__fn, 0x44160000u>()"
    )
    assert "Automation<>(startuptrigger_id" not in main_cpp
    # Both actions of the first block run in the one function, with no stop checks
    body = _function_body(main_cpp, "esphome__startuptrigger_id__fn")
    assert "boot high" in body
    assert "boot high lambda" in body
    assert "esphome__running" not in body
    assert "Automation" not in body
    assert "boot default" in _function_body(
        main_cpp, "esphome__startuptrigger_id_2__fn"
    )


def test_on_boot_delay_triggers_automation(main_cpp: str) -> None:
    """A delay keeps the Automation of separate actions, which the function triggers."""
    assert _constructed(main_cpp, "startuptrigger_id_3") == (
        "StartupCallback<esphome__startuptrigger_id_3__fn, 0xC2C80000u>()"
    )
    assert "new(automation_id_3) Automation<>();" in main_cpp
    assert "automation_id_3->add_action(delayaction_id);" in main_cpp
    assert _function_body(main_cpp, "esphome__startuptrigger_id_3__fn") == (
        "\n  ::automation_id_3->trigger();"
    )


def test_on_boot_manual_trigger_id_keeps_trigger(main_cpp: str) -> None:
    """A trigger_id set by hand can be used from lambdas, so the Trigger stays."""
    assert _constructed(main_cpp, "manual_boot") == "StartupTrigger(250.5f)"
    assert "new(automation_id_4) Automation<>(manual_boot);" in main_cpp


def test_on_shutdown_priorities(main_cpp: str) -> None:
    """on_shutdown calls its function at its priority; a delay keeps the Automation."""
    assert _constructed(main_cpp, "shutdowntrigger_id") == (
        "ShutdownCallback<esphome__shutdowntrigger_id__fn, 0x442F0000u>()"
    )
    assert "shutdown inline" in _function_body(
        main_cpp, "esphome__shutdowntrigger_id__fn"
    )
    assert _constructed(main_cpp, "shutdowntrigger_id_2") == (
        "ShutdownCallback<esphome__shutdowntrigger_id_2__fn, 0x44160000u>()"
    )
    assert "automation_id_6->add_action(delayaction_id_2);" in main_cpp
    assert "::automation_id_6->trigger();" in _function_body(
        main_cpp, "esphome__shutdowntrigger_id_2__fn"
    )


def test_on_loop(main_cpp: str) -> None:
    """on_loop calls its function each loop; a delay keeps the Automation."""
    assert _constructed(main_cpp, "looptrigger_id") == (
        "LoopCallback<esphome__looptrigger_id__fn>()"
    )
    assert "loop inline" in _function_body(main_cpp, "esphome__looptrigger_id__fn")
    assert "HasLoopOverride<LoopCallback<esphome__looptrigger_id__fn>>" in main_cpp
    assert _constructed(main_cpp, "looptrigger_id_2") == (
        "LoopCallback<esphome__looptrigger_id_2__fn>()"
    )
    assert "automation_id_8->add_action(delayaction_id_3);" in main_cpp
    assert "::automation_id_8->trigger();" in _function_body(
        main_cpp, "esphome__looptrigger_id_2__fn"
    )


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
