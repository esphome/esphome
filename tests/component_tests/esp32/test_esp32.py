"""
Test ESP32 configuration
"""

import asyncio
from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANTS,
    NetworkSdkconfigData,
    _reconcile_network_sdkconfig,
)
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_NETWORK_SDKCONFIG,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
)
from esphome.components.esp32.gpio import validate_gpio_pin
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_IGNORE_PIN_VALIDATION_ERROR,
    CONF_NUMBER,
    PlatformFramework,
    Toolchain,
)
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


def test_esp32_config(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.esp32 import CONFIG_SCHEMA, VARIANT_ESP32, VARIANT_FRIENDLY

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

    # Check that defining a variant sets the board name correctly.
    # Run under the ESP-IDF toolchain so variants without an entry in
    # STANDARD_BOARDS (S31, H4, H21) still derive a board name from
    # VARIANT_FRIENDLY rather than failing with cv.Invalid. CORE.toolchain
    # gets pinned by the first CONFIG_SCHEMA() call above (via
    # _resolve_toolchain) and that pinned value wins over the dict's
    # CONF_TOOLCHAIN, so clear it between iterations to mirror a fresh
    # config run.
    for variant in VARIANTS:
        CORE.toolchain = None
        config = CONFIG_SCHEMA(
            {
                "variant": variant,
                "toolchain": Toolchain.ESP_IDF.value,
            }
        )
        assert VARIANT_FRIENDLY[variant].lower() in config["board"]


@pytest.mark.parametrize(
    ("config_toolchain", "expected"),
    [
        # No `toolchain:` set -> the new default for esp32.
        (None, Toolchain.ESP_IDF),
        # An explicit `toolchain:` still wins over the default.
        (Toolchain.PLATFORMIO.value, Toolchain.PLATFORMIO),
        (Toolchain.ESP_IDF.value, Toolchain.ESP_IDF),
    ],
)
def test_esp32_default_toolchain_is_esp_idf(
    set_core_config: SetCoreConfigCallable,
    config_toolchain: str | None,
    expected: Toolchain,
) -> None:
    """With no `toolchain:` set (and nothing pinned via the CLI), esp32 resolves
    to the ESP-IDF toolchain; an explicit `toolchain:` still wins."""
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.esp32 import CONFIG_SCHEMA

    # Fresh run: no --toolchain CLI and no prior config pinned CORE.toolchain.
    CORE.toolchain = None
    config: dict[str, Any] = {"variant": VARIANT_ESP32}
    if config_toolchain is not None:
        config["toolchain"] = config_toolchain

    CONFIG_SCHEMA(config)

    assert CORE.toolchain == expected


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
            {"variant": "esp32s31", "toolchain": Toolchain.PLATFORMIO.value},
            r"No default board is known for ESP32S31\. Please specify the `board:` option explicitly\. @ data\['variant'\]",
            id="variant_without_default_board_requires_explicit_board_under_platformio",
        ),
        pytest.param(
            {
                "variant": "esp32s2",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' is not available on this esp32 variant @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
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
            id="execute_from_psram_requires_psram_s3_config",
        ),
        pytest.param(
            {
                "variant": "esp32p4",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' requires PSRAM to be configured @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_requires_psram_p4_config",
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


def test_execute_from_psram_s3_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that execute_from_psram on ESP32-S3 sets the correct sdkconfig options."""
    generate_main(component_config_path("execute_from_psram_s3.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_SPIRAM_FETCH_INSTRUCTIONS") is True
    assert sdkconfig.get("CONFIG_SPIRAM_RODATA") is True
    assert "CONFIG_SPIRAM_XIP_FROM_PSRAM" not in sdkconfig


def test_execute_from_psram_p4_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that execute_from_psram on ESP32-P4 sets the correct sdkconfig options."""
    generate_main(component_config_path("execute_from_psram_p4.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_SPIRAM_XIP_FROM_PSRAM") is True
    assert "CONFIG_SPIRAM_FETCH_INSTRUCTIONS" not in sdkconfig
    assert "CONFIG_SPIRAM_RODATA" not in sdkconfig


@pytest.mark.parametrize(
    ("fixture", "expect_warning"),
    [
        ("psram_quad_gpio34.yaml", False),
        ("psram_octal_gpio34.yaml", True),
        ("psram_octal_disabled_gpio34.yaml", False),
    ],
)
def test_s3_psram_pin_warning_only_for_octal(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    fixture: str,
    expect_warning: bool,
) -> None:
    """GPIO33-37 are only used by the PSRAM interface in octal mode.

    Using such a pin must only warn when octal PSRAM is configured; on quad
    PSRAM the pins are free and warning would be a false positive (#16857).
    """
    with caplog.at_level("WARNING"):
        generate_main(component_config_path(fixture))
    warned = "GPIO34 is used by the PSRAM interface in octal mode" in caplog.text
    assert warned == expect_warning


def test_ignore_pin_validation_error_on_clean_pin_warns(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A pin that passes validation but sets `ignore_pin_validation_error: true`
    should log a warning nudging the user to remove the flag, and not raise."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 4, CONF_IGNORE_PIN_VALIDATION_ERROR: True}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 4
    assert "GPIO4 has no validation errors to ignore" in caplog.text


def test_ignore_pin_validation_error_on_dirty_pin_suppresses(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A pin that fails validation with `ignore_pin_validation_error: true` should
    log the suppression warning and not raise (existing behavior)."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    # GPIO6 is a flash pin on ESP32 -> pin_validation raises cv.Invalid
    pin = {CONF_NUMBER: 6, CONF_IGNORE_PIN_VALIDATION_ERROR: True}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 6
    assert "Ignoring validation error on pin 6" in caplog.text


def test_dirty_pin_without_ignore_flag_raises(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A pin that fails validation without the ignore flag should still raise."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 6, CONF_IGNORE_PIN_VALIDATION_ERROR: False}
    with pytest.raises(cv.Invalid, match="flash interface"):
        validate_gpio_pin(pin)


def test_clean_pin_without_ignore_flag_does_not_warn(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A clean pin without the ignore flag should pass silently."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 4, CONF_IGNORE_PIN_VALIDATION_ERROR: False}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 4
    assert "has no validation errors to ignore" not in caplog.text


def test_execute_from_psram_disabled_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that without execute_from_psram, no XIP sdkconfig options are set."""
    generate_main(component_config_path("execute_from_psram_disabled.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert "CONFIG_SPIRAM_FETCH_INSTRUCTIONS" not in sdkconfig
    assert "CONFIG_SPIRAM_RODATA" not in sdkconfig
    assert "CONFIG_SPIRAM_XIP_FROM_PSRAM" not in sdkconfig


def test_platformio_idf_enables_reproducible_build(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test PlatformIO ESP-IDF builds enable reproducible app metadata."""
    generate_main(component_config_path("reproducible_build.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True


def test_platformio_arduino_enables_reproducible_build(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test PlatformIO Arduino builds enable reproducible app metadata."""
    generate_main(component_config_path("reproducible_build_arduino.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True


def test_native_idf_enables_reproducible_build(
    component_config_path: Callable[[str], Path],
) -> None:
    """Test native ESP-IDF builds enable reproducible app metadata."""
    from esphome.__main__ import generate_cpp_contents
    from esphome.config import read_config

    CORE.config_path = component_config_path("reproducible_build.yaml")
    CORE.config = read_config({})
    CORE.toolchain = Toolchain.ESP_IDF
    generate_cpp_contents(CORE.config)

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True


def test_flash_mode_sets_sdkconfig_and_pio_option(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """flash_mode/flash_frequency select the esptool flash parameters on both backends."""
    generate_main(component_config_path("flash_mode_idf.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_ESPTOOLPY_FLASHMODE_QIO") is True
    assert sdkconfig.get("CONFIG_ESPTOOLPY_FLASHFREQ_80M") is True
    assert CORE.platformio_options.get("board_build.flash_mode") == "qio"
    assert CORE.platformio_options.get("board_build.f_flash") == "80000000L"


def test_flash_mode_unset_leaves_defaults(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without flash_mode the board/sdkconfig defaults stay untouched."""
    generate_main(component_config_path("flash_mode_default.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert not any(key.startswith("CONFIG_ESPTOOLPY_FLASHMODE_") for key in sdkconfig)
    assert not any(key.startswith("CONFIG_ESPTOOLPY_FLASHFREQ_") for key in sdkconfig)
    assert "board_build.flash_mode" not in CORE.platformio_options
    assert "board_build.f_flash" not in CORE.platformio_options


@pytest.mark.parametrize(
    ("framework", "net", "preset", "expected"),
    [
        # --- IDF: single-interface cases (must match pre-refactor behavior) ---
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(wifi=True),
            {},
            {
                "CONFIG_ESP_WIFI_SOFTAP_SUPPORT": False,
                "CONFIG_LWIP_DHCPS": False,
            },
            id="idf_wifi_no_ap",
        ),
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(wifi=True, wifi_ap=True),
            {},
            {},
            id="idf_wifi_ap_leaves_softap_dhcps",
        ),
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(ethernet=True),
            {},
            {
                "CONFIG_ESP_WIFI_ENABLED": False,
                "CONFIG_SW_COEXIST_ENABLE": False,
            },
            id="idf_ethernet_only",
        ),
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(
                wifi=True, bluetooth=True, ble_42=True, software_coexistence=True
            ),
            {},
            {
                "CONFIG_BT_ENABLED": True,
                "CONFIG_BT_BLE_42_FEATURES_SUPPORTED": True,
                "CONFIG_SW_COEXIST_ENABLE": True,
                "CONFIG_ESP_WIFI_SOFTAP_SUPPORT": False,
                "CONFIG_LWIP_DHCPS": False,
            },
            id="idf_wifi_ble_tracker_coexistence",
        ),
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(bluetooth=True),
            {},
            {"CONFIG_BT_ENABLED": True},
            id="idf_ble_server_only_no_ble42",
        ),
        # --- IDF: user sdkconfig_options always win ---
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(wifi=True),
            {"CONFIG_ESP_WIFI_SOFTAP_SUPPORT": True},
            {
                "CONFIG_ESP_WIFI_SOFTAP_SUPPORT": True,
                "CONFIG_LWIP_DHCPS": False,
            },
            id="idf_user_override_wins",
        ),
        # --- IDF: user advanced enable_lwip_dhcp_server: false, even with AP ---
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(
                wifi=True, wifi_ap=True, enable_lwip_dhcp_server=False
            ),
            {},
            {"CONFIG_LWIP_DHCPS": False},
            id="idf_user_disables_dhcps_with_ap",
        ),
        # --- IDF: WiFi + Ethernet coexist (the multi-interface unlock) ---
        pytest.param(
            PlatformFramework.ESP32_IDF,
            NetworkSdkconfigData(wifi=True, ethernet=True),
            {},
            {
                "CONFIG_ESP_WIFI_SOFTAP_SUPPORT": False,
                "CONFIG_LWIP_DHCPS": False,
            },
            id="idf_wifi_and_ethernet_keeps_wifi_enabled",
        ),
        # --- Arduino: SoftAP/DHCPS disable is IDF-only ---
        pytest.param(
            PlatformFramework.ESP32_ARDUINO,
            NetworkSdkconfigData(wifi=True),
            {},
            {},
            id="arduino_wifi_no_ap_untouched",
        ),
        pytest.param(
            PlatformFramework.ESP32_ARDUINO,
            NetworkSdkconfigData(ethernet=True),
            {},
            {
                "CONFIG_ESP_WIFI_ENABLED": False,
                "CONFIG_SW_COEXIST_ENABLE": False,
            },
            id="arduino_ethernet_only_disables_wifi",
        ),
        # --- Arduino + Ethernet: DHCPS stays available even if user disabled it ---
        pytest.param(
            PlatformFramework.ESP32_ARDUINO,
            NetworkSdkconfigData(ethernet=True, enable_lwip_dhcp_server=False),
            {},
            {
                "CONFIG_ESP_WIFI_ENABLED": False,
                "CONFIG_SW_COEXIST_ENABLE": False,
            },
            id="arduino_ethernet_dhcps_exclusion",
        ),
    ],
)
def test_reconcile_network_sdkconfig(
    set_core_config: SetCoreConfigCallable,
    framework: PlatformFramework,
    net: NetworkSdkconfigData,
    preset: dict[str, Any],
    expected: dict[str, Any],
) -> None:
    """The FINAL-priority reconciler resolves WiFi/Ethernet/Bluetooth/coexistence
    sdkconfig flags from the requests recorded in NetworkSdkconfigData."""
    set_core_config(framework)
    CORE.data[KEY_ESP32] = {
        KEY_SDKCONFIG_OPTIONS: dict(preset),
        KEY_NETWORK_SDKCONFIG: net,
    }

    asyncio.run(_reconcile_network_sdkconfig())

    assert CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS] == expected


def test_network_wifi_only_reconciles_end_to_end(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """End-to-end: codegen for an ESP-IDF WiFi (no AP) config runs the reconciler
    after wifi's request_wifi(), disabling SoftAP support and the DHCP server."""
    generate_main(component_config_path("network_wifi_only.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is False
    assert sdkconfig.get("CONFIG_LWIP_DHCPS") is False
    # WiFi stack stays enabled (no ethernet) and no Bluetooth requested.
    assert "CONFIG_ESP_WIFI_ENABLED" not in sdkconfig
    assert "CONFIG_BT_ENABLED" not in sdkconfig


def test_network_ethernet_only_reconciles_end_to_end(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """End-to-end: ethernet's request_ethernet() makes the reconciler disable the
    WiFi stack and coexistence when WiFi is absent."""
    generate_main(component_config_path("network_ethernet_only.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_ESP_WIFI_ENABLED") is False
    assert sdkconfig.get("CONFIG_SW_COEXIST_ENABLE") is False


def test_network_wifi_ble_coexistence_reconciles_end_to_end(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """End-to-end: WiFi + esp32_ble_tracker software_coexistence resolves to
    BT enabled and coexistence on, with SoftAP/DHCP server dropped (no AP)."""
    generate_main(component_config_path("network_wifi_ble_coexistence.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_BT_ENABLED") is True
    assert sdkconfig.get("CONFIG_BT_BLE_42_FEATURES_SUPPORTED") is True
    assert sdkconfig.get("CONFIG_SW_COEXIST_ENABLE") is True
    assert sdkconfig.get("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is False
    assert sdkconfig.get("CONFIG_LWIP_DHCPS") is False
    # WiFi present alongside BT -> WiFi stack must stay enabled.
    assert "CONFIG_ESP_WIFI_ENABLED" not in sdkconfig
