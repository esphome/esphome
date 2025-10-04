"""
Test ESP32 configuration
"""

from typing import Any

import pytest

from esphome.components.esp32 import VARIANTS
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_esp32_config(
    set_core_config: SetCoreConfigCallable,
) -> None:
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
        pytest.param(
            {
                "variant": "esp32s2",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' is only supported on ESP32S3 variant @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_invalid_for_variant_config",
        ),
        pytest.param(
            {
                "variant": "esp32s3",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' requires PSRAM to be configured @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_requires_psram_config",
        ),
        pytest.param(
            {
                "variant": "esp32s3",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"ignore_efuse_mac_crc": True},
                },
            },
            r"'ignore_efuse_mac_crc' is not supported on ESP32S3 @ data\['framework'\]\['advanced'\]\['ignore_efuse_mac_crc'\]",
            id="ignore_efuse_mac_crc_only_on_esp32",
        ),
    ],
)
def test_esp32_configuration_errors(
    config: Any,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={CONF_ESPHOME: {}})
    """Test detection of invalid configuration."""
    from esphome.components.esp32 import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    with pytest.raises(cv.Invalid, match=error_match):
        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))
