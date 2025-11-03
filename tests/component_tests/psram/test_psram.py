"""Tests for PSRAM component."""

from typing import Any

import pytest

from esphome.components.esp32.const import (
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

UNSUPPORTED_PSRAM_VARIANTS = [
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
]

SUPPORTED_PSRAM_VARIANTS = [
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32P4,
]


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {},
            r"PSRAM is not supported on this chip",
            id="psram_not_supported",
        ),
    ],
)
@pytest.mark.parametrize("variant", UNSUPPORTED_PSRAM_VARIANTS)
def test_psram_configuration_errors_unsupported_variants(
    config: Any,
    error_match: str,
    variant: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={CONF_ESPHOME: {}},
    )
    """Test detection of invalid PSRAM configuration on unsupported variants."""
    from esphome.components.psram import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize("variant", SUPPORTED_PSRAM_VARIANTS)
def test_psram_configuration_valid_supported_variants(
    variant: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={
            CONF_ESPHOME: {},
            "esp32": {
                "variant": variant,
                "cpu_frequency": "160MHz",
                "framework": {"type": "esp-idf"},
            },
        },
    )
    """Test that PSRAM configuration is valid on supported variants."""
    from esphome.components.psram import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    # This should not raise an exception
    config = CONFIG_SCHEMA({})
    FINAL_VALIDATE_SCHEMA(config)


def _setup_psram_final_validation_test(
    esp32_config: dict,
    set_core_config: SetCoreConfigCallable,
    set_component_config: Any,
) -> str:
    """Helper function to set up ESP32 configuration for PSRAM final validation tests."""
    # Use ESP32S3 for schema validation to allow all options, then override for final validation
    schema_variant = "ESP32S3"
    final_variant = esp32_config.get("variant", "ESP32S3")
    full_esp32_config = {
        "variant": final_variant,
        "cpu_frequency": esp32_config.get("cpu_frequency", "240MHz"),
        "framework": {"type": "esp-idf"},
    }

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: schema_variant},
        full_config={
            CONF_ESPHOME: {},
            "esp32": full_esp32_config,
        },
    )
    set_component_config("esp32", full_esp32_config)

    return final_variant


@pytest.mark.parametrize(
    ("config", "esp32_config", "expect_error", "error_match"),
    [
        pytest.param(
            {"speed": "120MHz"},
            {"cpu_frequency": "160MHz"},
            True,
            r"PSRAM 120MHz requires 240MHz CPU frequency",
            id="120mhz_requires_240mhz_cpu",
        ),
        pytest.param(
            {"mode": "octal"},
            {"variant": "ESP32"},
            True,
            r"Octal PSRAM is only supported on ESP32-S3",
            id="octal_mode_only_esp32s3",
        ),
        pytest.param(
            {"mode": "quad", "enable_ecc": True},
            {},
            True,
            r"ECC is only available in octal mode",
            id="ecc_only_in_octal_mode",
        ),
        pytest.param(
            {"speed": "120MHZ"},
            {"cpu_frequency": "240MHZ"},
            False,
            None,
            id="120mhz_with_240mhz_cpu",
        ),
        pytest.param(
            {"mode": "octal"},
            {"variant": "ESP32S3"},
            False,
            None,
            id="octal_mode_on_esp32s3",
        ),
        pytest.param(
            {"mode": "octal", "enable_ecc": True},
            {"variant": "ESP32S3"},
            False,
            None,
            id="ecc_in_octal_mode",
        ),
    ],
)
def test_psram_final_validation(
    config: Any,
    esp32_config: dict,
    expect_error: bool,
    error_match: str | None,
    set_core_config: SetCoreConfigCallable,
    set_component_config: Any,
) -> None:
    """Test PSRAM final validation for both error and valid cases."""
    from esphome.components.psram import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
    from esphome.core import CORE

    final_variant = _setup_psram_final_validation_test(
        esp32_config, set_core_config, set_component_config
    )

    validated_config = CONFIG_SCHEMA(config)

    # Update CORE variant for final validation
    CORE.data["esp32"][KEY_VARIANT] = final_variant

    if expect_error:
        with pytest.raises(cv.Invalid, match=error_match):
            FINAL_VALIDATE_SCHEMA(validated_config)
    else:
        # This should not raise an exception
        FINAL_VALIDATE_SCHEMA(validated_config)
