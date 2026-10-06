"""Tests for the Home Assistant action field tables."""

from collections.abc import Callable
from pathlib import Path

CONFIG = "tests/component_tests/api/test_homeassistant_fields.yaml"


def test_strings_are_flash_arrays_on_esp8266(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each distinct string is emitted once as its own PROGMEM array."""
    main_cpp = generate_main(CONFIG)

    assert main_cpp.count('PROGMEM = "message";') == 1
    assert main_cpp.count('PROGMEM = "notify.notify";') == 1
    assert 'ESPHOME_F("message")' not in main_cpp


def test_constant_tables_are_shared(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Identical constant actions use one table."""
    main_cpp = generate_main(CONFIG)

    assert main_cpp.count("(api_apiserver_id, false, ha_action_fields, 1, 0, 0);") == 2


def test_tables_with_lambdas_are_not_shared(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A lambda may keep static state, so each action keeps its own table."""
    main_cpp = generate_main(CONFIG)

    assert "(api_apiserver_id, true, ha_action_fields_2, 1, 0, 0);" in main_cpp
    assert "(api_apiserver_id, true, ha_action_fields_3, 1, 0, 0);" in main_cpp
