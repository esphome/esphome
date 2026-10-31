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


def test_on_boot_inline_priorities(main_cpp: str) -> None:
    """All-inline on_boot blocks call one function from setup() at their priority, default 600.

    The priority is the float's bit pattern: 0x44480000 is 800.0f, 0x44160000 is 600.0f.
    """
    assert _constructed(main_cpp, "startuptrigger_id") == (
        "StartupCallback<esphome__lambdaaction_id__fn, 0x44480000u>()"
    )
    assert _constructed(main_cpp, "startuptrigger_id_2") == (
        "StartupCallback<esphome__lambdaaction_id_3__fn, 0x44160000u>()"
    )
    assert "Automation<>(startuptrigger_id)" not in main_cpp
    assert "Automation<>(startuptrigger_id_2)" not in main_cpp
    # Both actions of the first block run in the one function, with no stop checks
    match = re.search(
        r"static void esphome__lambdaaction_id__fn\(\) \{(.*?)\n\}\n",
        main_cpp,
        re.DOTALL,
    )
    assert match is not None
    body = match.group(1)
    assert "boot high" in body
    assert "boot high lambda" in body
    assert "esphome__running" not in body


def test_on_boot_delay_keeps_trigger(main_cpp: str) -> None:
    """A delay needs the Trigger and its Automation of separate actions."""
    assert _constructed(main_cpp, "startuptrigger_id_3") == "StartupTrigger(-100.0f)"
    assert "new(automation_id_3) Automation<>(startuptrigger_id_3);" in main_cpp
    assert "automation_id_3->add_action(delayaction_id);" in main_cpp


def test_on_boot_manual_trigger_id_keeps_trigger(main_cpp: str) -> None:
    """A trigger_id set by hand can be used from lambdas, so the Trigger stays."""
    assert _constructed(main_cpp, "manual_boot") == "StartupTrigger(250.5f)"
    assert "new(automation_id_4) Automation<>(manual_boot);" in main_cpp


def test_on_shutdown_priorities(main_cpp: str) -> None:
    """All-inline on_shutdown calls one function at its priority; a delay keeps the Trigger."""
    assert _constructed(main_cpp, "shutdowntrigger_id") == (
        "ShutdownCallback<esphome__lambdaaction_id_7__fn, 0x442F0000u>()"
    )
    assert "Automation<>(shutdowntrigger_id)" not in main_cpp
    assert _constructed(main_cpp, "shutdowntrigger_id_2") == "ShutdownTrigger(600.0f)"
    assert "automation_id_6->add_action(delayaction_id_2);" in main_cpp


def test_on_loop(main_cpp: str) -> None:
    """All-inline on_loop calls one function each loop; a delay keeps the Trigger."""
    assert _constructed(main_cpp, "looptrigger_id") == (
        "LoopCallback<esphome__lambdaaction_id_9__fn>()"
    )
    assert "Automation<>(looptrigger_id)" not in main_cpp
    assert "HasLoopOverride<LoopCallback<esphome__lambdaaction_id_9__fn>>" in main_cpp
    assert _constructed(main_cpp, "looptrigger_id_2") == "LoopTrigger()"
    assert "automation_id_8->add_action(delayaction_id_3);" in main_cpp


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
