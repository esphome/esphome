"""Matrix keypad key codes live in a shared PROGMEM table instead of a std::string."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.matrix_keypad import check_keys
import esphome.config_validation as cv


def test_keys_use_shared_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("keys.yaml"))

    assert (
        "static constexpr uint8_t matrix_keypad_keys[] PROGMEM = {49, 50, 51, 52};"
        in main_cpp
    )
    assert main_cpp.count("->set_keys(matrix_keypad_keys);") == 2


def test_non_ascii_keys_are_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'é' is not an ASCII"):
        check_keys({"keys": "1é34"})
