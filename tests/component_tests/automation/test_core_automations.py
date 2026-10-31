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
    """All-inline on_boot blocks keep their configured priority, default 600."""
    assert _constructed(main_cpp, "startuptrigger_id") == "StartupTrigger(800.0f)"
    assert _constructed(main_cpp, "startuptrigger_id_2") == "StartupTrigger(600.0f)"
    assert "Automation<>(startuptrigger_id)" in main_cpp
    assert "Automation<>(startuptrigger_id_2)" in main_cpp


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
    """on_shutdown blocks keep their priority; a delay keeps the Trigger."""
    assert _constructed(main_cpp, "shutdowntrigger_id") == "ShutdownTrigger(700.0f)"
    assert "Automation<>(shutdowntrigger_id)" in main_cpp
    assert _constructed(main_cpp, "shutdowntrigger_id_2") == "ShutdownTrigger(600.0f)"
    assert "automation_id_6->add_action(delayaction_id_2);" in main_cpp


def test_on_loop(main_cpp: str) -> None:
    """on_loop blocks are looping components; a delay keeps the Trigger."""
    assert _constructed(main_cpp, "looptrigger_id") == "LoopTrigger()"
    assert "Automation<>(looptrigger_id)" in main_cpp
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
