"""SX1509 keypad key codes live in a shared PROGMEM table instead of a std::string."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.sx1509 import check_keys
import esphome.config_validation as cv


def test_keys_use_shared_progmem_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("keys.yaml"))

    assert (
        "static constexpr uint8_t sx1509_keys[] PROGMEM = {97, 98, 99, 100};"
        in main_cpp
    )
    assert main_cpp.count("->set_keys(sx1509_keys);") == 2


def test_non_ascii_keys_are_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'é' is not an ASCII"):
        check_keys({"keys": "1é34"})
