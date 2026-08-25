"""
Test ESP32 configuration
"""

import asyncio
from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.esp32 import (
    KEY_FATFS_REQUIRED,
    KEY_VFS_DIR_REQUIRED,
    KEY_VFS_SELECT_REQUIRED,
    KEY_VFS_TERMIOS_REQUIRED,
    VARIANT_ESP32,
    VARIANTS,
    NetworkSdkconfigData,
    RawSdkconfigValue,
    _ota_downgrade_protection_errors,
    _reconcile_network_sdkconfig,
    _reconcile_vfs_fatfs_sdkconfig,
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
    "config_toolchain",
    [Toolchain.SDK_NRF.value, "nonsense"],
)
def test_esp32_rejects_unsupported_toolchains(
    set_core_config: SetCoreConfigCallable,
    config_toolchain: str,
) -> None:
    """Toolchains esp32 does not support are rejected at validation time."""
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.esp32 import CONFIG_SCHEMA

    CORE.toolchain = None
    with pytest.raises(cv.Invalid, match="Unknown value"):
        CONFIG_SCHEMA({"variant": VARIANT_ESP32, "toolchain": config_toolchain})


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
        pytest.param(
            {
                "variant": "esp32",
                "board": "esp32dev",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"nvs_encryption": {"key_id": 0}},
                },
            },
            r"NVS encryption \(HMAC scheme\) is not supported on ESP32 .* @ data\['framework'\]\['advanced'\]\['nvs_encryption'\]",
            id="nvs_encryption_unsupported_on_esp32",
        ),
        pytest.param(
            {
                "variant": "esp32s3",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"nvs_encryption": {"key_id": 6}},
                },
            },
            r"value must be at most 5 .* @ data\['framework'\]\['advanced'\]\['nvs_encryption'\]\['key_id'\]",
            id="nvs_encryption_key_id_out_of_range",
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


@pytest.mark.parametrize(
    ("config_file", "reincluded"),
    [
        pytest.param(
            "exclusion_reincludes.yaml",
            ("esp_driver_i2c", "esp_driver_ledc", "esp_driver_gptimer"),
            id="i2c_ledc_ac_dimmer",
        ),
        # esp-tls has three owners; a per-owner config makes a dropped
        # re-include from any single one fail the test.
        pytest.param(
            "exclusion_reincludes_http_request.yaml",
            ("esp-tls", "esp_http_client"),
            id="http_request",
        ),
        pytest.param(
            # "mqtt" itself is deliberately not asserted: on IDF >= 6.0 it
            # is a managed component and never leaves the exclusion set.
            "exclusion_reincludes_mqtt.yaml",
            ("esp-tls",),
            id="mqtt",
        ),
        pytest.param(
            "exclusion_reincludes_web_server.yaml",
            ("esp-tls", "esp_http_server"),
            id="web_server_idf",
        ),
        pytest.param(
            "nvs_encryption_s3.yaml",
            ("nvs_sec_provider",),
            id="nvs_encryption",
        ),
        pytest.param(
            "exclusion_reincludes_nvs_sdkconfig.yaml",
            ("nvs_sec_provider",),
            id="nvs_encryption_raw_sdkconfig",
        ),
        pytest.param(
            "exclusion_reincludes_camera_web_server.yaml",
            ("esp_http_server",),
            id="esp32_camera_web_server",
        ),
        pytest.param(
            "exclusion_reincludes_nextion.yaml",
            ("esp-tls", "esp_http_client"),
            id="nextion",
        ),
    ],
)
def test_default_exclusions_reincluded_by_owning_components(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    reincluded: tuple[str, ...],
) -> None:
    """Components whose IDF driver is excluded by default must re-include it
    during codegen; a dropped include_builtin_idf_component() call would only
    surface as a missing-header failure in a full compile job."""
    from esphome.components.esp32.const import KEY_EXCLUDE_COMPONENTS

    generate_main(component_config_path(config_file))
    excluded = CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]

    for name in reincluded:
        assert name not in excluded, f"{name} should have been re-included"

    # Components no part of this config touches stay excluded.
    assert "unity" in excluded
    assert "fatfs" in excluded
    # The HTTP server only comes back for configs that run one.
    assert ("esp_http_server" in excluded) == ("esp_http_server" not in reincluded)


def test_nvs_sec_provider_stays_excluded_when_encryption_is_off(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """An explicit CONFIG_NVS_ENCRYPTION=n keeps nvs_sec_provider excluded."""
    from esphome.components.esp32.const import KEY_EXCLUDE_COMPONENTS

    generate_main(component_config_path("exclusion_stays_nvs_sdkconfig_off.yaml"))
    assert "nvs_sec_provider" in CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS]


_BUNDLE_OPTIONS = (
    "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE",
    "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN",
    "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL",
)


@pytest.mark.parametrize(
    ("config_file", "expected"),
    [
        pytest.param("exclusion_reincludes.yaml", (False, None, None), id="no_tls"),
        pytest.param(
            "certificate_bundle_http_request.yaml",
            (True, True, False),
            id="http_request",
        ),
        pytest.param(
            "exclusion_reincludes_http_request.yaml",
            (False, None, None),
            id="http_request_no_verify",
        ),
        pytest.param(
            "certificate_bundle_full.yaml", (True, None, True), id="full_option"
        ),
        pytest.param(
            "certificate_bundle_arduino_tls.yaml",
            (True, True, False),
            id="arduino_network_client_secure",
        ),
    ],
)
def test_certificate_bundle_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected: tuple[bool | None, ...],
) -> None:
    """The bundle and its CMN/FULL variant are written only when requested."""
    generate_main(component_config_path(config_file))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert tuple(sdkconfig.get(name) for name in _BUNDLE_OPTIONS) == expected


def test_user_sdkconfig_certificate_bundle_wins(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A raw sdkconfig_options bundle setting is kept and still pins CMN."""
    generate_main(component_config_path("certificate_bundle_sdkconfig.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    value = sdkconfig["CONFIG_MBEDTLS_CERTIFICATE_BUNDLE"]
    assert isinstance(value, RawSdkconfigValue)
    assert value.value == "y"
    assert sdkconfig.get("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN") is True
    assert sdkconfig.get("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL") is False


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


def test_nvs_encryption_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Test that nvs_encryption sets the HMAC scheme sdkconfig options."""
    generate_main(component_config_path("nvs_encryption_s3.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_NVS_ENCRYPTION") is True
    assert sdkconfig.get("CONFIG_NVS_SEC_KEY_PROTECT_USING_HMAC") is True
    assert sdkconfig.get("CONFIG_NVS_SEC_HMAC_EFUSE_KEY_ID") == 0
    # The permanent/irreversible eFuse burn is warned about at config time.
    assert "PERMANENT and IRREVERSIBLE" in caplog.text


@pytest.mark.parametrize(
    ("fixture", "multi_key", "idf_on_update"),
    [
        # Externally-signed RSA with a declared trusted-key list hands
        # verification to ESPHome's multi-key verifier, so IDF's single-block
        # on-update check must be OFF. It defaults ON under
        # SECURE_SIGNED_APPS_NO_SECURE_BOOT, so it has to be set to False
        # explicitly -- not merely omitted.
        ("signed_ota_verification_keys_s3.yaml", True, False),
        # Externally-signed RSA without a trusted-key list has no trust anchor,
        # so it falls back to IDF's built-in check.
        ("signed_ota_external_rsa_s3.yaml", False, True),
        # Build-time signing and the other schemes keep IDF's check.
        ("signed_ota_signing_key_s3.yaml", False, True),
        ("signed_ota_ecdsa256_c6.yaml", False, True),
        ("signed_ota_ecdsa_v1.yaml", False, True),
    ],
)
def test_signed_ota_verification_sdkconfig(
    fixture: str,
    multi_key: bool,
    idf_on_update: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only external RSA disables IDF's on-update check and uses ESPHome's verifier."""
    generate_main(component_config_path(fixture))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    # The padded, externally-signable image is always produced.
    assert sdkconfig.get("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT") is True
    # Explicit value (never left to the Kconfig default) decides who verifies.
    assert (
        sdkconfig.get("CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT") is idf_on_update
    )
    defines = {define.name for define in CORE.defines}
    assert ("USE_OTA_SIGNED_VERIFICATION_MULTI_KEY" in defines) is multi_key
    if multi_key:
        # The padding / reserved signature sector the verifier depends on keys
        # off the RSA scheme symbol, not the hidden CONFIG_SECURE_SIGNED_APPS
        # (which the explicit `n` above drives to n). Pin the real dependency.
        assert sdkconfig.get("CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME") is True
        # The compiled-in trust anchor: the fixture lists one key.
        define_values = {define.name: str(define.value) for define in CORE.defines}
        assert define_values["OTA_TRUSTED_KEY_COUNT"] == "1"
        assert "OTA_TRUSTED_KEY_DIGESTS" in define_values


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
            NetworkSdkconfigData(wifi=True, bluetooth=True, software_coexistence=True),
            {},
            {
                "CONFIG_BT_ENABLED": True,
                "CONFIG_BT_BLE_42_FEATURES_SUPPORTED": True,
                "CONFIG_BT_BLE_50_FEATURES_SUPPORTED": False,
                "CONFIG_SW_COEXIST_ENABLE": True,
                "CONFIG_ESP_WIFI_SOFTAP_SUPPORT": False,
                "CONFIG_LWIP_DHCPS": False,
            },
            id="idf_wifi_ble_tracker_coexistence",
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


@pytest.mark.parametrize(
    ("requires", "fatfs_required", "disables", "preset", "expected"),
    [
        # Nothing required and every disable_* flag off (NOT the shipped defaults, which
        # disable everything): VFS enabled, FATFS left untouched entirely.
        pytest.param(
            {},
            False,
            (False, False, False, False),
            {},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
            },
            id="nothing_disabled_nothing_required",
        ),
        # The shipped out-of-the-box path: every disable_* flag defaults to True and nothing
        # is required -- VFS off, FATFS at the smallest footprint (8.3 names, one volume).
        pytest.param(
            {},
            False,
            (True, True, True, True),
            {},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": False,
                "CONFIG_VFS_SUPPORT_SELECT": False,
                "CONFIG_VFS_SUPPORT_DIR": False,
                "CONFIG_FATFS_LFN_NONE": True,
                "CONFIG_FATFS_VOLUME_COUNT": 1,
            },
            id="all_disabled_fatfs_fallback",
        ),
        # A component's require_* beats the user's disable_* flag for every VFS feature.
        pytest.param(
            {
                KEY_VFS_TERMIOS_REQUIRED: True,
                KEY_VFS_SELECT_REQUIRED: True,
                KEY_VFS_DIR_REQUIRED: True,
            },
            False,
            (True, True, True, False),
            {},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
            },
            id="require_beats_disable",
        ),
        # A user sdkconfig_options preset wins over a require (the set_opt guard).
        pytest.param(
            {KEY_VFS_SELECT_REQUIRED: True},
            False,
            (False, False, False, False),
            {"CONFIG_VFS_SUPPORT_SELECT": False},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": False,
                "CONFIG_VFS_SUPPORT_DIR": True,
            },
            id="user_preset_wins_over_require",
        ),
        # require_fatfs() with no user preset: long filenames on the heap, 255 chars,
        # four volumes.
        pytest.param(
            {},
            True,
            (False, False, False, False),
            {},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
                "CONFIG_FATFS_LFN_NONE": False,
                "CONFIG_FATFS_LFN_HEAP": True,
                "CONFIG_FATFS_MAX_LFN": 255,
                "CONFIG_FATFS_VOLUME_COUNT": 4,
            },
            id="fatfs_required_defaults",
        ),
        # CONFIG_FATFS_LONG_FILENAMES is a Kconfig choice: a user picking any member
        # (here LFN_STACK) leaves the whole group untouched -- no second =y in the choice.
        pytest.param(
            {},
            True,
            (False, False, False, False),
            {"CONFIG_FATFS_LFN_STACK": "y"},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
                "CONFIG_FATFS_LFN_STACK": "y",
                "CONFIG_FATFS_VOLUME_COUNT": 4,
            },
            id="fatfs_user_lfn_stack_untouched",
        ),
        # disable_fatfs (the shipped default) with a user LFN pick: the choice group is the
        # user's -- no LFN_NONE=y written next to their member, only the volume fallback.
        pytest.param(
            {},
            False,
            (False, False, False, True),
            {"CONFIG_FATFS_LFN_HEAP": "y"},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
                "CONFIG_FATFS_LFN_HEAP": "y",
                "CONFIG_FATFS_VOLUME_COUNT": 1,
            },
            id="disable_fatfs_user_lfn_untouched",
        ),
        # Same for an explicit LFN_NONE preset: the group is the user's, only the volume
        # count default is added.
        pytest.param(
            {},
            True,
            (False, False, False, False),
            {"CONFIG_FATFS_LFN_NONE": "y"},
            {
                "CONFIG_VFS_SUPPORT_TERMIOS": True,
                "CONFIG_VFS_SUPPORT_SELECT": True,
                "CONFIG_VFS_SUPPORT_DIR": True,
                "CONFIG_FATFS_LFN_NONE": "y",
                "CONFIG_FATFS_VOLUME_COUNT": 4,
            },
            id="fatfs_user_lfn_none_untouched",
        ),
    ],
)
def test_reconcile_vfs_fatfs_sdkconfig(
    set_core_config: SetCoreConfigCallable,
    requires: dict[str, bool],
    fatfs_required: bool,
    disables: tuple[bool, bool, bool, bool],
    preset: dict[str, Any],
    expected: dict[str, Any],
) -> None:
    """The FINAL-priority reconciler resolves the VFS feature flags and the FATFS
    defaults from the recorded require_* calls, with user sdkconfig_options winning
    and the LFN Kconfig choice treated as one group."""
    set_core_config(PlatformFramework.ESP32_IDF)
    CORE.data[KEY_ESP32] = {KEY_SDKCONFIG_OPTIONS: dict(preset)}
    if fatfs_required:
        CORE.data[KEY_ESP32][KEY_FATFS_REQUIRED] = True
    for key, value in requires.items():
        CORE.data[key] = value

    asyncio.run(_reconcile_vfs_fatfs_sdkconfig(*disables))

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
    assert sdkconfig.get("CONFIG_BT_BLE_50_FEATURES_SUPPORTED") is False
    assert sdkconfig.get("CONFIG_SW_COEXIST_ENABLE") is True
    assert sdkconfig.get("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is False
    assert sdkconfig.get("CONFIG_LWIP_DHCPS") is False
    # WiFi present alongside BT -> WiFi stack must stay enabled.
    assert "CONFIG_ESP_WIFI_ENABLED" not in sdkconfig


def test_network_wifi_ethernet_priority_keeps_wifi_enabled(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """End-to-end: with both WiFi and Ethernet declared under network: priority:,
    the reconciler must NOT disable the WiFi stack or coexistence (the
    multi-interface case unlocked by composing network priority with the
    sdkconfig reconciler)."""
    generate_main(component_config_path("network_wifi_ethernet_priority.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert "CONFIG_ESP_WIFI_ENABLED" not in sdkconfig
    assert "CONFIG_SW_COEXIST_ENABLE" not in sdkconfig
    # WiFi has no AP here, so SoftAP/DHCP server are still dropped.
    assert sdkconfig.get("CONFIG_ESP_WIFI_SOFTAP_SUPPORT") is False
    assert sdkconfig.get("CONFIG_LWIP_DHCPS") is False


def test_esp32_build_internals_are_yaml_only() -> None:
    """ESP32 raw framework / build inputs are ``YAML_ONLY``.

    The framework block's PlatformIO package pins, raw ESP-IDF
    sdkconfig options, the low-level ``advanced`` block, extra IDF
    component sources, plus the partition table and toolchain override
    on the main schema are build internals — never UI form fields.
    User-facing choices (framework type/version, board, variant, …)
    stay on the main form.
    """
    from esphome.components.esp32 import CONFIG_SCHEMA, FRAMEWORK_SCHEMA

    fw_markers = {str(k): k for k in FRAMEWORK_SCHEMA.schema}
    for field in (
        "release",
        "source",
        "platform_version",
        "sdkconfig_options",
        "advanced",
        "components",
    ):
        assert fw_markers[field].visibility is cv.Visibility.YAML_ONLY, field
    # Framework type/version remain user-facing.
    assert fw_markers["type"].visibility is None
    assert fw_markers["version"].visibility is None

    main_markers = {str(k): k for k in CONFIG_SCHEMA.validators[0].schema}
    assert main_markers["partitions"].visibility is cv.Visibility.YAML_ONLY
    # toolchain is a real but rarely-touched override -> advanced disclosure.
    assert main_markers["toolchain"].visibility is cv.Visibility.ADVANCED
    assert main_markers["board"].visibility is None
    assert main_markers["flash_size"].visibility is None


def test_downgrade_protection_passes_with_numeric_version_and_signing() -> None:
    assert _ota_downgrade_protection_errors("1.2.3", signed_ota_enabled=True) == []


def test_downgrade_protection_accepts_calendar_version() -> None:
    assert _ota_downgrade_protection_errors("2024.12.0", signed_ota_enabled=True) == []


def test_downgrade_protection_requires_project_version() -> None:
    errs = _ota_downgrade_protection_errors(None, signed_ota_enabled=True)
    assert len(errs) == 1
    assert "version" in str(errs[0])


def test_downgrade_protection_rejects_non_numeric_version() -> None:
    errs = _ota_downgrade_protection_errors("1.0-beta", signed_ota_enabled=True)
    assert len(errs) == 1
    assert "dotted-numeric" in str(errs[0])


def test_downgrade_protection_requires_signed_ota() -> None:
    errs = _ota_downgrade_protection_errors("1.2.3", signed_ota_enabled=False)
    assert len(errs) == 1
    assert "signed_ota_verification" in str(errs[0])


def test_downgrade_protection_reports_all_unmet_requirements() -> None:
    # No project version and no signing -> two distinct errors.
    errs = _ota_downgrade_protection_errors(None, signed_ota_enabled=False)
    assert len(errs) == 2


@pytest.mark.parametrize(
    "config",
    [
        # V2 schemes: signing key (sign during build) or no key at all
        # (external signing; the public key travels in the signature block).
        {"signing_scheme": "rsa3072", "signing_key": "key.pem"},
        {"signing_scheme": "rsa3072"},
        {"signing_scheme": "ecdsa256", "signing_key": "key.pem"},
        {"signing_scheme": "ecdsa256"},
        # V1 ECDSA: exactly one of signing key / verification key.
        {"signing_scheme": "ecdsa_v1", "signing_key": "key.pem"},
        {"signing_scheme": "ecdsa_v1", "verification_key": "key.bin"},
        # External RSA with a compiled-in trusted-key list (digests).
        {"signing_scheme": "rsa3072", "verification_keys": ["ab" * 32]},
        {"signing_scheme": "rsa3072", "verification_keys": ["ab" * 32, "cd" * 32]},
    ],
)
def test_signed_ota_keys_valid_combinations(config: dict) -> None:
    from esphome.components.esp32 import _validate_signed_ota_keys

    assert _validate_signed_ota_keys(config) is config


@pytest.mark.parametrize("value", [None, {}])
def test_signed_ota_bare_block_selects_v2_external_signing(value: dict | None) -> None:
    """A bare `signed_ota_verification:` block is valid: the default V2
    scheme embeds the public key in the signature block, so verifying
    externally-signed binaries needs no keys in the config."""
    from esphome.components.esp32 import _validate_signed_ota_verification

    config = _validate_signed_ota_verification(value)
    assert config == {"signing_scheme": "rsa3072"}


@pytest.mark.parametrize(
    ("config", "match"),
    [
        # A verification key is meaningless with the V2 schemes -- the public
        # key is embedded in each image's signature block.
        (
            {"signing_scheme": "rsa3072", "verification_key": "key.bin"},
            "only used with signing scheme 'ecdsa_v1'",
        ),
        (
            {"signing_scheme": "ecdsa256", "verification_key": "key.bin"},
            "only used with signing scheme 'ecdsa_v1'",
        ),
        # V1 ECDSA needs a key either way.
        (
            {"signing_scheme": "ecdsa_v1"},
            "Signing scheme 'ecdsa_v1' requires either",
        ),
        # Never both keys at once.
        (
            {
                "signing_scheme": "rsa3072",
                "signing_key": "key.pem",
                "verification_key": "key.bin",
            },
            "not both",
        ),
        (
            {
                "signing_scheme": "ecdsa_v1",
                "signing_key": "key.pem",
                "verification_key": "key.bin",
            },
            "not both",
        ),
        # A trusted-key list only applies to external RSA.
        (
            {"signing_scheme": "ecdsa256", "verification_keys": ["ab" * 32]},
            "only used with signing scheme 'rsa3072'",
        ),
        # Can't both auto-sign and verify against a fixed trusted set.
        (
            {
                "signing_scheme": "rsa3072",
                "signing_key": "key.pem",
                "verification_keys": ["ab" * 32],
            },
            "cannot be combined with",
        ),
        # The singular V1 key and the RSA trusted-key list are mutually exclusive.
        (
            {
                "signing_scheme": "rsa3072",
                "verification_key": "key.bin",
                "verification_keys": ["ab" * 32],
            },
            "at most one",
        ),
        # Duplicate trusted keys are rejected.
        (
            {"signing_scheme": "rsa3072", "verification_keys": ["ab" * 32, "ab" * 32]},
            "must be unique",
        ),
    ],
)
def test_signed_ota_keys_invalid_combinations(config: dict, match: str) -> None:
    from esphome.components.esp32 import _validate_signed_ota_keys

    with pytest.raises(cv.Invalid, match=match):
        _validate_signed_ota_keys(config)


def test_sbv2_rsa_key_digest_known_answer() -> None:
    """The compiled-in trust anchor is the block-format digest the device
    computes per signature block; pin it to espsecure's known output for the
    shipped dummy key so a future change to the derivation can't drift silently.
    """
    from esphome.components.esp32 import _sbv2_rsa_key_digest

    key = (
        Path(__file__).parent.parent.parent
        / "components"
        / "esp32"
        / "dummy_signing_key.pem"
    )
    assert (
        _sbv2_rsa_key_digest(key).hex()
        == "957671f5ec1b55b3fb1d32c5525a68d3b8c33847922daddb4feefe64cd679f65"
    )


def test_validate_trusted_key_hex_forms() -> None:
    """The digest-input branch: the same key as an uppercase 64-hex digest
    normalizes to the PEM-derived value (the two forms are interchangeable), and
    a mangled digest fails clearly instead of as a missing file.
    """
    from esphome.components.esp32 import _sbv2_rsa_key_digest, _validate_trusted_key

    key = (
        Path(__file__).parent.parent.parent
        / "components"
        / "esp32"
        / "dummy_signing_key.pem"
    )
    pem_digest = _sbv2_rsa_key_digest(key).hex()
    assert _validate_trusted_key(pem_digest.upper()) == pem_digest
    for bad in (pem_digest[:-1], "0x" + pem_digest):
        with pytest.raises(cv.Invalid, match="64 hex"):
            _validate_trusted_key(bad)
    # An unquoted 0x.../all-digit digest reaches the validator as a YAML int.
    with pytest.raises(cv.Invalid, match="Quote the digest"):
        _validate_trusted_key(0x957671F5EC1B55B3)


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        # Full x.y.z versions are rewritten into pioarduino release URLs
        (
            "55.3.30",
            "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.30/platform-espressif32.zip",
        ),
        (
            "55.3.31-2",
            "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.31-2/platform-espressif32.zip",
        ),
        # Non-version values pass through untouched
        (
            "https://github.com/pioarduino/platform-espressif32.git#develop",
            "https://github.com/pioarduino/platform-espressif32.git#develop",
        ),
    ],
)
def test_parse_pio_platform_version(value: str, expected: str) -> None:
    from esphome.components.esp32 import _parse_pio_platform_version

    assert _parse_pio_platform_version(value) == expected
