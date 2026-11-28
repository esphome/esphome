"""Tests for the homeassistant number platform."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.config import read_config
from esphome.core import CORE

VALID_CONFIG = "tests/component_tests/homeassistant/test_number.yaml"
INVALID_CONFIG = "tests/component_tests/homeassistant/test_number_invalid_domain.yaml"


def test_number_and_input_number_entities(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Both number and input_number entity IDs are accepted and passed to the C++ class."""
    main_cpp = generate_main(VALID_CONFIG)

    assert 'ha_number->set_entity_id("number.some_number");' in main_cpp
    assert 'ha_input_number->set_entity_id("input_number.some_input");' in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)


def test_number_ranges_stay_out_of_flash_tables(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Home Assistant numbers get their bounds at run time; a copy starts on the shared zero range."""
    main_cpp = generate_main(VALID_CONFIG)

    for var in ("ha_number", "ha_input_number"):
        assert re.search(rf"\b{var}->set_range\(", main_cpp) is None
        assert re.search(rf"\b{var}->traits\.set_min_value\(", main_cpp) is None
    # Zero bounds until setup() shares the source's range, as before ranges moved to flash
    assert "copy_of_ha_number->set_range(" in main_cpp
    assert "copy_of_ha_number->traits.set_min_value(" not in main_cpp


def test_number_rejects_unsupported_domain(
    capsys: pytest.CaptureFixture[str],
) -> None:
    """An entity ID from another domain fails config validation."""
    CORE.config_path = Path(INVALID_CONFIG)

    assert read_config({}) is None
    assert "is not supported by the number platform" in capsys.readouterr().out
