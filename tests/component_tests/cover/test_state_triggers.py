"""Tests for the cover on_opened, on_closed, on_opening, on_closing and on_idle triggers."""

from collections.abc import Callable
from pathlib import Path


def test_state_triggers_share_one_callback(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("state_triggers.yaml"))

    assert main_cpp.count("door->add_on_state_callback(") == 1
    assert "door->add_on_state_callback(esphome__door__on_state_callback);" in main_cpp
    body = main_cpp.split("static void esphome__door__on_state_callback() {")[1]
    body = body.split("\n}")[0]
    # Same order as dev's trigger objects: opened, closed, closing, opening, idle.
    checks = [
        "::door->position == cover::COVER_OPEN",
        "::door->position == cover::COVER_CLOSED",
        "::door->current_operation == cover::COVER_OPERATION_CLOSING",
        "::door->current_operation == cover::COVER_OPERATION_OPENING",
        "::door->current_operation == cover::COVER_OPERATION_IDLE",
    ]
    keys = ["on_opened", "on_closed", "on_closing", "on_opening", "on_idle"]
    for key, check in zip(keys, checks, strict=True):
        assert f"static bool door_{key}_0_last;" in body
        assert (
            f"if (esphome::internal::change_trigger::entered(door_{key}_0_last, ({check})))"
            in body
        )
    positions = [body.index(f"({check})") for check in checks]
    assert positions == sorted(positions)
    assert "Trigger" not in main_cpp
    assert "blind->add_on_state_callback(" not in main_cpp


def test_nothing_emitted_without_state_triggers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("no_state_triggers.yaml"))

    assert "on_state_callback" not in main_cpp
    assert "add_on_state_callback" not in main_cpp
