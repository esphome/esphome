"""Tests for the cover on_opened, on_closed, on_opening, on_closing and on_idle triggers."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

DEFINE = "USE_COVER_STATE_TRIGGERS"


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


def test_state_triggers_register_entered_callbacks(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("state_triggers.yaml"))

    assert main_cpp.count("door->add_on_state_entered_callback(") == 5
    for bit in ("OPEN", "CLOSED", "OPENING", "CLOSING", "IDLE"):
        assert f"entered & cover::COVER_ENTERED_{bit}" in main_cpp
    assert "blind->add_on_state_entered_callback(" not in main_cpp
    assert "Trigger" not in main_cpp
    assert DEFINE in _defines()


def test_nothing_emitted_without_state_triggers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("no_state_triggers.yaml"))

    assert "add_on_state_entered_callback" not in main_cpp
    assert DEFINE not in _defines()
