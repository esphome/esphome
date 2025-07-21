"""
Test ESP32 configuration
"""

from typing import Any

import pytest

from esphome.components.esp32 import VARIANTS
import esphome.config_validation as cv
from esphome.const import PlatformFramework


def test_esp32_config(set_core_config) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.esp32 import CONFIG_SCHEMA
    from esphome.components.esp32.const import VARIANT_ESP32, VARIANT_FRIENDLY

    # Example ESP32 configuration
    config = {
        "board": "esp32dev",
        "variant": VARIANT_ESP32,
        "cpu_frequency": "240MHz",
        "flash_size": "4MB",
        "framework": {
            "type": "esp-idf",
        },
    }

    # Check if the variant is valid
    config = CONFIG_SCHEMA(config)
    assert config["variant"] == VARIANT_ESP32

    # Check that defining a variant sets the board name correctly
    for variant in VARIANTS:
        config = CONFIG_SCHEMA(
            {
                "variant": variant,
            }
        )
        assert VARIANT_FRIENDLY[variant].lower() in config["board"]


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {"flash_size": "4MB"},
            r"This board is unknown, if you are sure you want to compile with this board selection, override with option 'variant' @ data\['board'\]",
            id="unknown_board_config",
        ),
        pytest.param(
            {"variant": "esp32xx"},
            r"Unknown value 'ESP32XX', did you mean 'ESP32', 'ESP32S3', 'ESP32S2'\? for dictionary value @ data\['variant'\]",
            id="unknown_variant_config",
        ),
        pytest.param(
            {"variant": "esp32s3", "board": "esp32dev"},
            r"Option 'variant' does not match selected board. @ data\['variant'\]",
            id="mismatched_board_variant_config",
        ),
    ],
)
def test_esp32_configuration_errors(
    config: Any,
    error_match: str,
) -> None:
    """Test detection of invalid configuration."""
    from esphome.components.esp32 import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)
