"""Tests for the cover on_opened, on_closed, on_opening, on_closing and on_idle triggers."""

from collections.abc import Callable
from pathlib import Path


def test_state_triggers_share_one_callback(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("state_triggers.yaml"))

    assert main_cpp.count("door->add_on_state_callback(") == 1
    assert "door->add_on_state_callback(esphome__door__state_triggers);" in main_cpp
    body = main_cpp.split("static void esphome__door__state_triggers() {")[1]
    body = body.split("\n}")[0]
    assert "static uint8_t seen_0 = 0;" in body
    # Same order as dev's trigger objects: opened, closed, closing, opening, idle.
    checks = [
        "0x01, ::door->position == cover::COVER_OPEN)",
        "0x02, ::door->position == cover::COVER_CLOSED)",
        "0x04, ::door->current_operation == cover::COVER_OPERATION_CLOSING)",
        "0x08, ::door->current_operation == cover::COVER_OPERATION_OPENING)",
        "0x10, ::door->current_operation == cover::COVER_OPERATION_IDLE)",
    ]
    positions = [
        body.index(f"cover::state_entered(seen_0, {check}") for check in checks
    ]
    assert positions == sorted(positions)
    assert "Trigger" not in main_cpp
    assert "blind->add_on_state_callback(" not in main_cpp


def test_ninth_automation_uses_a_second_byte(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("many_state_triggers.yaml"))

    assert "static uint8_t seen_1 = 0;" in main_cpp
    assert "cover::state_entered(seen_0, 0x80," in main_cpp
    assert "cover::state_entered(seen_1, 0x01," in main_cpp
    assert "seen_2" not in main_cpp


def test_nothing_emitted_without_state_triggers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("no_state_triggers.yaml"))

    assert "state_triggers" not in main_cpp
    assert "add_on_state_callback" not in main_cpp
