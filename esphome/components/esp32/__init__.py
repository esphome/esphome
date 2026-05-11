import contextlib
from dataclasses import dataclass
import itertools
import logging
import os
from pathlib import Path
import re
import subprocess

from esphome import yaml_util
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_COMPONENTS,
    CONF_DISABLED,
    CONF_ENABLE_FULL_PRINTF,
    CONF_ENABLE_OTA_ROLLBACK,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_IGNORE_EFUSE_CUSTOM_MAC,
    CONF_IGNORE_EFUSE_MAC_CRC,
    CONF_LOG_LEVEL,
    CONF_NAME,
    CONF_OTA,
    CONF_PATH,
    CONF_PLATFORM_VERSION,
    CONF_PLATFORMIO_OPTIONS,
    CONF_REF,
    CONF_SAFE_MODE,
    CONF_SIZE,
    CONF_SOURCE,
    CONF_TOOLCHAIN,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    CONF_WATCHDOG_TIMEOUT,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_NAME,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    ThreadModel,
    Toolchain,
    __version__,
)
from esphome.core import CORE, HexInt, Library
from esphome.core.config import BOARD_MAX_LENGTH
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.espidf.component import generate_idf_component
import esphome.final_validate as fv
from esphome.helpers import copy_file_if_changed, rmtree, write_file_if_changed
from esphome.types import ConfigType
from esphome.writer import clean_build, clean_cmake_cache

from .boards import BOARDS, STANDARD_BOARDS
from .const import (  # noqa
    KEY_ARDUINO_LIBRARIES,
    KEY_BOARD,
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_EXCLUDE_COMPONENTS,
    KEY_EXTRA_BUILD_FILES,
    KEY_FLASH_SIZE,
    KEY_FULL_CERT_BUNDLE,
    KEY_IDF_VERSION,
    KEY_PATH,
    KEY_REF,
    KEY_REPO,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_FRIENDLY,
    VARIANTS,
)

# force import gpio to register pin schema
from .gpio import esp32_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["preferences"]
CODEOWNERS = ["@esphome/core"]
IS_TARGET_PLATFORM = True

CONF_ASSERTION_LEVEL = "assertion_level"
CONF_COMPILER_OPTIMIZATION = "compiler_optimization"
CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES = "enable_idf_experimental_features"
CONF_ENGINEERING_SAMPLE = "engineering_sample"
CONF_INCLUDE_BUILTIN_IDF_COMPONENTS = "include_builtin_idf_components"
CONF_ENABLE_LWIP_ASSERT = "enable_lwip_assert"
CONF_EXECUTE_FROM_PSRAM = "execute_from_psram"
CONF_MINIMUM_CHIP_REVISION = "minimum_chip_revision"
CONF_RELEASE = "release"
CONF_SIGNED_OTA_VERIFICATION = "signed_ota_verification"
CONF_SIGNING_KEY = "signing_key"
CONF_SIGNING_SCHEME = "signing_scheme"
CONF_SRAM1_AS_IRAM = "sram1_as_iram"
CONF_SUBTYPE = "subtype"
CONF_VERIFICATION_KEY = "verification_key"

ARDUINO_FRAMEWORK_NAME = "framework-arduinoespressif32"
ARDUINO_FRAMEWORK_PKG = f"pioarduino/{ARDUINO_FRAMEWORK_NAME}"
ARDUINO_LIBS_NAME = f"{ARDUINO_FRAMEWORK_NAME}-libs"
ARDUINO_LIBS_PKG = f"pioarduino/{ARDUINO_LIBS_NAME}"

LOG_LEVELS_IDF = [
    "NONE",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "VERBOSE",
]

ASSERTION_LEVELS = {
    "DISABLE": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE",
    "ENABLE": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE",
    "SILENT": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT",
}

SIGNING_SCHEMES = {
    "rsa3072": "CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME",
    "ecdsa256": "CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME",
    "ecdsa_v1": "CONFIG_SECURE_SIGNED_APPS_ECDSA_SCHEME",
}

# Chip variants that only support one V2 signing scheme.
# Based on SOC_SECURE_BOOT_V2_RSA / SOC_SECURE_BOOT_V2_ECC in soc_caps.h.
# Variants not listed in either set support both RSA and ECDSA V2
# (e.g. C5, C6, H2, P4). New variants should be added to the
# appropriate set if they only support one scheme.
# Note: VARIANT_ESP32 is not listed here because it supports V2 RSA only
# when minimum_chip_revision >= 3.0, which requires special handling.
SIGNED_OTA_V2_RSA_ONLY_VARIANTS = {
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32C3,
}
SIGNED_OTA_V2_ECC_ONLY_VARIANTS = {
    VARIANT_ESP32C2,
    VARIANT_ESP32C61,
}
# V1 ECDSA (Secure Boot V1) is only supported on the original ESP32.
# Based on SOC_SECURE_BOOT_V1 in soc_caps.h.
SIGNED_OTA_V1_ECDSA_VARIANTS = {
    VARIANT_ESP32,
}

COMPILER_OPTIMIZATIONS = {
    "DEBUG": "CONFIG_COMPILER_OPTIMIZATION_DEBUG",
    "NONE": "CONFIG_COMPILER_OPTIMIZATION_NONE",
    "PERF": "CONFIG_COMPILER_OPTIMIZATION_PERF",
    "SIZE": "CONFIG_COMPILER_OPTIMIZATION_SIZE",
}

# ESP-IDF components excluded by default to reduce compile time.
# Components can be re-enabled by calling include_builtin_idf_component() in to_code().
#
# Cannot be excluded (dependencies of required components):
# - "console": espressif/mdns unconditionally depends on it
# - "sdmmc": driver -> esp_driver_sdmmc -> sdmmc dependency chain
DEFAULT_EXCLUDED_IDF_COMPONENTS = (
    "cmock",  # Unit testing mock framework - ESPHome doesn't use IDF's testing
    "driver",  # Legacy driver shim - only needed by esp32_touch, esp32_can for legacy headers
    "esp_adc",  # ADC driver - only needed by adc component
    "esp_driver_dac",  # DAC driver - only needed by esp32_dac component
    "esp_driver_i2s",  # I2S driver - only needed by i2s_audio component
    "esp_driver_mcpwm",  # MCPWM driver - ESPHome doesn't use motor control PWM
    "esp_driver_pcnt",  # PCNT driver - only needed by pulse_counter, hlw8012 components
    "esp_driver_rmt",  # RMT driver - only needed by remote_transmitter/receiver, neopixelbus
    "esp_driver_touch_sens",  # Touch sensor driver - only needed by esp32_touch
    "esp_driver_twai",  # TWAI/CAN driver - only needed by esp32_can component
    "esp_eth",  # Ethernet driver - only needed by ethernet component
    "esp_hid",  # HID host/device support - ESPHome doesn't implement HID functionality
    "esp_http_client",  # HTTP client - only needed by http_request component
    "esp_https_ota",  # ESP-IDF HTTPS OTA - ESPHome has its own OTA implementation
    "esp_https_server",  # HTTPS server - ESPHome has its own web server
    "esp_lcd",  # LCD controller drivers - only needed by display component
    "esp_local_ctrl",  # Local control over HTTPS/BLE - ESPHome has native API
    "espcoredump",  # Core dump support - ESPHome has its own debug component
    "fatfs",  # FAT filesystem - ESPHome doesn't use filesystem storage
    "mqtt",  # ESP-IDF MQTT library - ESPHome has its own MQTT implementation
    "openthread",  # Thread protocol - only needed by openthread component
    "perfmon",  # Xtensa performance monitor - ESPHome has its own debug component
    "protocomm",  # Protocol communication for provisioning - unused by ESPHome
    "spiffs",  # SPIFFS filesystem - ESPHome doesn't use filesystem storage (IDF only)
    "ulp",  # ULP coprocessor - not currently used by any ESPHome component
    "unity",  # Unit testing framework - ESPHome doesn't use IDF's testing
    "wear_levelling",  # Flash wear levelling for fatfs - unused since fatfs unused
    "wifi_provisioning",  # WiFi provisioning - ESPHome uses its own improv implementation
)

# Additional IDF managed components to exclude for Arduino framework builds
# These are pulled in by the Arduino framework's idf_component.yml but not used by ESPHome
# Note: Component names include the namespace prefix (e.g., "espressif__cbor") because
# that's how managed components are registered in the IDF build system
# List includes direct dependencies from arduino-esp32/idf_component.yml
# plus transitive dependencies from RainMaker/Insights (except espressif/mdns which we need)
ARDUINO_EXCLUDED_IDF_COMPONENTS = (
    "chmorgan__esp-libhelix-mp3",  # MP3 decoder - not used
    "espressif__cbor",  # CBOR library - only used by RainMaker/Insights
    "espressif__esp-dsp",  # DSP library - not used
    "espressif__esp-modbus",  # Modbus - ESPHome has its own
    "espressif__esp-sr",  # Speech recognition - not used
    "espressif__esp-zboss-lib",  # Zigbee ZBOSS library - not used
    "espressif__esp-zigbee-lib",  # Zigbee library - not used
    "espressif__esp_diag_data_store",  # Diagnostics - not used
    "espressif__esp_diagnostics",  # Diagnostics - not used
    "espressif__esp_hosted",  # ESP hosted - only for ESP32-P4
    "espressif__esp_insights",  # ESP Insights - not used
    "espressif__esp_modem",  # Modem library - not used
    "espressif__esp_rainmaker",  # RainMaker - not used
    "espressif__esp_rcp_update",  # RCP update - RainMaker transitive dep
    "espressif__esp_schedule",  # Schedule - RainMaker transitive dep
    "espressif__esp_secure_cert_mgr",  # Secure cert - RainMaker transitive dep
    "espressif__esp_wifi_remote",  # WiFi remote - only for ESP32-P4
    "espressif__json_generator",  # JSON generator - RainMaker transitive dep
    "espressif__json_parser",  # JSON parser - RainMaker transitive dep
    "espressif__lan867x",  # Ethernet PHY - ESPHome uses ESP-IDF ethernet directly
    "espressif__libsodium",  # Crypto - ESPHome uses its own noise-c library
    "espressif__network_provisioning",  # Network provisioning - not used
    "espressif__qrcode",  # QR code - not used
    "espressif__rmaker_common",  # RainMaker common - not used
    "joltwallet__littlefs",  # LittleFS - ESPHome doesn't use filesystem
)

# Mapping of Arduino libraries to IDF managed components they require
# When an Arduino library is enabled via cg.add_library(), these components
# are automatically un-stubbed from ARDUINO_EXCLUDED_IDF_COMPONENTS.
#
# Note: Some libraries (Matter, LittleFS, ESP_SR, WiFiProv, ArduinoOTA) already have
# conditional maybe_add_component() calls in arduino-esp32/CMakeLists.txt that handle
# their managed component dependencies. Our mapping is primarily needed for libraries
# that don't have such conditionals (Ethernet, PPP, Zigbee, RainMaker, Insights, etc.)
# and to ensure the stubs are removed from our idf_component.yml overrides.
ARDUINO_LIBRARY_IDF_COMPONENTS: dict[str, tuple[str, ...]] = {
    "BLE": ("esp_driver_gptimer",),
    "BluetoothSerial": ("esp_driver_gptimer",),
    "ESP_HostedOTA": ("espressif__esp_hosted", "espressif__esp_wifi_remote"),
    "ESP_SR": ("espressif__esp-sr",),
    "Ethernet": ("espressif__lan867x",),
    "FFat": ("fatfs",),
    "Insights": (
        "espressif__cbor",
        "espressif__esp_insights",
        "espressif__esp_diagnostics",
        "espressif__esp_diag_data_store",
        "espressif__rmaker_common",  # Transitive dep from esp_insights
    ),
    "LittleFS": ("joltwallet__littlefs",),
    "Matter": ("espressif__esp_matter",),
    "PPP": ("espressif__esp_modem",),
    "RainMaker": (
        # Direct deps from idf_component.yml
        "espressif__cbor",
        "espressif__esp_rainmaker",
        "espressif__esp_insights",
        "espressif__esp_diagnostics",
        "espressif__esp_diag_data_store",
        "espressif__rmaker_common",
        "espressif__qrcode",
        # Transitive deps from esp_rainmaker
        "espressif__esp_rcp_update",
        "espressif__esp_schedule",
        "espressif__esp_secure_cert_mgr",
        "espressif__json_generator",
        "espressif__json_parser",
        "espressif__network_provisioning",
    ),
    "SD": ("fatfs",),
    "SD_MMC": ("fatfs",),
    "SPIFFS": ("spiffs",),
    "WiFiProv": ("espressif__network_provisioning", "espressif__qrcode"),
    "Zigbee": ("espressif__esp-zigbee-lib", "espressif__esp-zboss-lib"),
}

# Arduino library to Arduino library dependencies
# When enabling one library, also enable its dependencies
# Kconfig "select" statements don't work with CONFIG_ARDUINO_SELECTIVE_COMPILATION
ARDUINO_LIBRARY_DEPENDENCIES: dict[str, tuple[str, ...]] = {
    "Ethernet": ("Network",),
    "WiFi": ("Network",),
}


def _idf_component_stub_name(component: str) -> str:
    """Get stub directory name from IDF component name.

    Component names are typically namespace__name (e.g., espressif__cbor).
    Returns just the name part (e.g., cbor). If no namespace is present,
    returns the original component name.
    """
    _prefix, sep, suffix = component.partition("__")
    return suffix if sep else component


def _idf_component_dep_name(component: str) -> str:
    """Convert IDF component name to dependency format.

    Converts espressif__cbor to espressif/cbor.
    """
    return component.replace("__", "/")


# Arduino libraries to disable by default when using Arduino framework
# ESPHome uses ESP-IDF APIs directly; we only need the Arduino core
# (HardwareSerial, Print, Stream, GPIO functions which are always compiled)
# Components use cg.add_library() which auto-enables any they need
# This list must match ARDUINO_ALL_LIBRARIES from arduino-esp32/CMakeLists.txt
ARDUINO_DISABLED_LIBRARIES: frozenset[str] = frozenset(
    {
        "ArduinoOTA",
        "AsyncUDP",
        "BLE",
        "BluetoothSerial",
        "DNSServer",
        "EEPROM",
        "ESP_HostedOTA",
        "ESP_I2S",
        "ESP_NOW",
        "ESP_SR",
        "ESPmDNS",
        "Ethernet",
        "FFat",
        "FS",
        "Hash",
        "HTTPClient",
        "HTTPUpdate",
        "Insights",
        "LittleFS",
        "Matter",
        "NetBIOS",
        "Network",
        "NetworkClientSecure",
        "OpenThread",
        "PPP",
        "Preferences",
        "RainMaker",
        "SD",
        "SD_MMC",
        "SimpleBLE",
        "SPI",
        "SPIFFS",
        "Ticker",
        "Update",
        "USB",
        "WebServer",
        "WiFi",
        "WiFiProv",
        "Wire",
        "Zigbee",
    }
)

# ESP32 (original) chip revision options
# Setting minimum revision to 3.0 or higher:
# - Reduces flash size by excluding workaround code for older chip bugs
# - For PSRAM users: disables CONFIG_SPIRAM_CACHE_WORKAROUND, which saves significant
#   IRAM by keeping C library functions in ROM instead of recompiling them
# See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/chip_revision.html
ESP32_CHIP_REVISIONS = {
    "0.0": "CONFIG_ESP32_REV_MIN_0",
    "1.0": "CONFIG_ESP32_REV_MIN_1",
    "1.1": "CONFIG_ESP32_REV_MIN_1_1",
    "2.0": "CONFIG_ESP32_REV_MIN_2",
    "3.0": "CONFIG_ESP32_REV_MIN_3",
    "3.1": "CONFIG_ESP32_REV_MIN_3_1",
}

# Socket limit configuration for ESP-IDF
# ESP-IDF CONFIG_LWIP_MAX_SOCKETS has range 1-253, default 10
DEFAULT_MAX_SOCKETS = 10  # ESP-IDF default

ARDUINO_ALLOWED_VARIANTS = [
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
]


def get_cpu_frequencies(*frequencies: int) -> list[str]:
    return [f"{frequency}MHZ" for frequency in frequencies]


CPU_FREQUENCIES = {
    VARIANT_ESP32: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32C2: get_cpu_frequencies(80, 120),
    VARIANT_ESP32C3: get_cpu_frequencies(80, 160),
    VARIANT_ESP32C5: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32C6: get_cpu_frequencies(80, 120, 160),
    VARIANT_ESP32C61: get_cpu_frequencies(80, 120, 160),
    VARIANT_ESP32H2: get_cpu_frequencies(16, 32, 48, 64, 96),
    VARIANT_ESP32P4: get_cpu_frequencies(40, 360, 400),
    VARIANT_ESP32S2: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32S3: get_cpu_frequencies(80, 160, 240),
}

# Make sure not missed here if a new variant added.
assert all(variant in CPU_FREQUENCIES for variant in VARIANTS)

FULL_CPU_FREQUENCIES = set(itertools.chain.from_iterable(CPU_FREQUENCIES.values()))


def set_core_data(config):
    cpu_frequency = config.get(CONF_CPU_FREQUENCY, None)
    variant = config[CONF_VARIANT]
    # if not specified in config, default to the maximum supported frequency
    # (ESP32-P4 engineering samples are limited to 360MHz, non-engineering can do 400MHz)
    if cpu_frequency is None:
        choices = CPU_FREQUENCIES[variant]
        if variant == VARIANT_ESP32P4 and config.get(CONF_ENGINEERING_SAMPLE):
            cpu_frequency = "360MHZ"
        else:
            cpu_frequency = choices[-1]
        config[CONF_CPU_FREQUENCY] = cpu_frequency
    elif cpu_frequency not in CPU_FREQUENCIES[variant]:
        raise cv.Invalid(
            f"Invalid CPU frequency '{cpu_frequency}' for {config[CONF_VARIANT]}",
            path=[CONF_CPU_FREQUENCY],
        )

    if variant == VARIANT_ESP32P4 and cpu_frequency == "400MHZ":
        _LOGGER.warning(
            "400MHz on ESP32-P4 is experimental and may not boot. "
            "Consider using 360MHz instead. See https://github.com/esphome/esphome/issues/13425"
        )

    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_ESP32
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
        if variant not in ARDUINO_ALLOWED_VARIANTS:
            raise cv.Invalid(
                f"ESPHome does not support using the Arduino framework for the {variant}. Please use the ESP-IDF framework instead.",
                path=[CONF_FRAMEWORK, CONF_TYPE],
            )
    CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS] = {}
    CORE.data[KEY_ESP32][KEY_COMPONENTS] = {}
    # Initialize with default exclusions - components can call include_builtin_idf_component()
    # to re-enable any they need
    excluded = set(DEFAULT_EXCLUDED_IDF_COMPONENTS)
    # Add Arduino-specific managed component exclusions when using Arduino framework
    if conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        excluded.update(ARDUINO_EXCLUDED_IDF_COMPONENTS)
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS] = excluded
    # Initialize Arduino library tracking - cg.add_library() auto-enables libraries
    CORE.data[KEY_ESP32][KEY_ARDUINO_LIBRARIES] = set()
    framework_ver = cv.Version.parse(config[CONF_FRAMEWORK][CONF_VERSION])
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = framework_ver

    # Store the underlying IDF version for framework-agnostic checks
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        CORE.data[KEY_ESP32][KEY_IDF_VERSION] = framework_ver
    elif (idf_ver := ARDUINO_IDF_VERSION_LOOKUP.get(framework_ver)) is not None:
        if CORE.using_toolchain_esp_idf:
            # Official ESP-IDF frameworks don't use extra
            idf_ver = cv.Version(idf_ver.major, idf_ver.minor, idf_ver.patch)
        CORE.data[KEY_ESP32][KEY_IDF_VERSION] = idf_ver
    else:
        raise cv.Invalid(
            f"Arduino version {framework_ver} has no known ESP-IDF version mapping. "
            "Please update ARDUINO_IDF_VERSION_LOOKUP.",
            path=[CONF_FRAMEWORK, CONF_VERSION],
        )

    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ESP32][KEY_FLASH_SIZE] = config[CONF_FLASH_SIZE]
    CORE.data[KEY_ESP32][KEY_VARIANT] = variant
    CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES] = {}

    return config


def get_esp32_variant(core_obj=None):
    return (core_obj or CORE).data[KEY_ESP32][KEY_VARIANT]


def get_board(core_obj=None):
    return (core_obj or CORE).data[KEY_ESP32][KEY_BOARD]


def get_download_types(storage_json):
    """Binary-download entries for a built ESP32 firmware.

    Used by:
    - esphome.dashboard (legacy "Download .bin" button)
    - device-builder (esphome/device-builder) — same dispatch via
      ``importlib.import_module(f"esphome.components.{platform}")``
      then ``module.get_download_types(storage)``. The contract is
      "returns ``list[dict]`` with at least ``title`` /
      ``description`` / ``file`` / ``download`` keys"; please keep
      the shape stable so the new dashboard's download panel
      doesn't have to special-case per-platform schemas.
    """
    return [
        {
            "title": "Factory format (Previously Modern)",
            "description": "For use with ESPHome Web and other tools.",
            "file": "firmware.factory.bin",
            "download": f"{storage_json.name}.factory.bin",
        },
        {
            "title": "OTA format (Previously Legacy)",
            "description": "For OTA updating a device.",
            "file": "firmware.ota.bin",
            "download": f"{storage_json.name}.ota.bin",
        },
    ]


def only_on_variant(*, supported=None, unsupported=None, msg_prefix="This feature"):
    """Config validator for features only available on some ESP32 variants."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj):
        if not CORE.is_esp32:
            raise cv.Invalid(f"{msg_prefix} is only available on ESP32")
        variant = get_esp32_variant()
        if supported is not None and variant not in supported:
            raise cv.Invalid(
                f"{msg_prefix} is only available on {', '.join(supported)}"
            )
        if unsupported is not None and variant in unsupported:
            raise cv.Invalid(
                f"{msg_prefix} is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


@dataclass
class RawSdkconfigValue:
    """An sdkconfig value that won't be auto-formatted"""

    value: str


SdkconfigValueType = bool | int | HexInt | str | RawSdkconfigValue


def add_idf_sdkconfig_option(name: str, value: SdkconfigValueType):
    """Set an esp-idf sdkconfig value."""
    CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][name] = value


def add_idf_component(
    *,
    name: str,
    repo: str | None = None,
    ref: str | None = None,
    path: str | None = None,
):
    """Add an esp-idf component to the project."""
    if not repo and not ref and not path:
        raise ValueError("Requires at least one of repo, ref or path")
    components_registry = CORE.data[KEY_ESP32][KEY_COMPONENTS]
    existing = components_registry.get(name)
    if existing and existing.get(KEY_REF) != ref:
        _LOGGER.warning(
            "IDF component %s version conflict %s replaced by %s",
            name,
            existing.get(KEY_REF),
            ref,
        )
    components_registry[name] = {
        KEY_REPO: repo,
        KEY_REF: ref,
        KEY_PATH: path,
    }


def exclude_builtin_idf_component(name: str) -> None:
    """Exclude an ESP-IDF component from the build.

    This reduces compile time by skipping components that are not needed.
    The component will be passed to ESP-IDF's EXCLUDE_COMPONENTS cmake variable.

    Note: Components that are dependencies of other required components
    cannot be excluded - ESP-IDF will still build them.
    """
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS].add(name)


def include_builtin_idf_component(name: str) -> None:
    """Remove an ESP-IDF component from the exclusion list.

    Call this from components that need an ESP-IDF component that is
    excluded by default in DEFAULT_EXCLUDED_IDF_COMPONENTS. This ensures the
    component will be built when needed.
    """
    CORE.data[KEY_ESP32][KEY_EXCLUDE_COMPONENTS].discard(name)


def _enable_arduino_library(name: str) -> None:
    """Enable an Arduino library that is disabled by default.

    This is called automatically by CORE.add_library() when a component adds
    an Arduino library via cg.add_library(). Components should not call this
    directly - just use cg.add_library("LibName", None).

    Args:
        name: The library name (e.g., "Wire", "SPI", "WiFi")
    """
    enabled_libs: set[str] = CORE.data[KEY_ESP32][KEY_ARDUINO_LIBRARIES]
    enabled_libs.add(name)
    # Also enable any required Arduino library dependencies
    for dep_lib in ARDUINO_LIBRARY_DEPENDENCIES.get(name, ()):
        enabled_libs.add(dep_lib)
    # Also enable any required IDF components
    for idf_component in ARDUINO_LIBRARY_IDF_COMPONENTS.get(name, ()):
        include_builtin_idf_component(idf_component)


def add_extra_script(stage: str, filename: str, path: Path):
    """Add an extra script to the project."""
    key = f"{stage}:{filename}"
    if add_extra_build_file(filename, path):
        cg.add_platformio_option("extra_scripts", [key])


def add_extra_build_file(filename: str, path: Path) -> bool:
    """Add an extra build file to the project."""
    if filename not in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES]:
        CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES][filename] = {
            KEY_NAME: filename,
            KEY_PATH: path,
        }
        return True
    return False


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # 3.3.6+ changed filename from esp32-{ver}.zip to esp32-core-{ver}.tar.xz
    if ver >= cv.Version(3, 3, 6):
        filename = f"esp32-core-{ver}.tar.xz"
    else:
        filename = f"esp32-{ver}.zip"
    return f"{ARDUINO_FRAMEWORK_PKG}@https://github.com/espressif/arduino-esp32/releases/download/{ver}/{filename}"


def _format_framework_pio_espidf_version(
    ver: cv.Version, release: str | None = None
) -> str:
    # format the given espidf (https://github.com/pioarduino/esp-idf/releases) version to
    # a PIO platformio/framework-espidf value
    if ver == cv.Version(5, 4, 3) or ver >= cv.Version(5, 5, 1):
        ext = "tar.xz"
    else:
        ext = "zip"
    # Build version string with extra separator based on type:
    # numeric extra uses dot (e.g., "5.5.3.1"), string extra uses dash (e.g., "6.0.0-rc1")
    ver_str = f"{ver.major}.{ver.minor}.{ver.patch}"
    if ver.extra:
        sep = "." if str(ver.extra).isdigit() else "-"
        ver_str += f"{sep}{ver.extra}"
    if release:
        return f"pioarduino/framework-espidf@https://github.com/pioarduino/esp-idf/releases/download/v{ver_str}.{release}/esp-idf-v{ver_str}.{ext}"
    return f"pioarduino/framework-espidf@https://github.com/pioarduino/esp-idf/releases/download/v{ver_str}/esp-idf-v{ver_str}.{ext}"


def _is_framework_url(source: str) -> bool:
    # platformio accepts many URL schemes for framework repositories and archives including http, https, git, file, and symlink
    import urllib.parse

    try:
        parsed = urllib.parse.urlparse(source)
    except ValueError:
        return False
    return bool(parsed.scheme)


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)

# The default/recommended arduino framework version
#  - https://github.com/espressif/arduino-esp32/releases
ARDUINO_FRAMEWORK_VERSION_LOOKUP = {
    "recommended": cv.Version(3, 3, 8),
    "latest": cv.Version(3, 3, 8),
    "dev": cv.Version(3, 3, 8),
}
ARDUINO_PLATFORM_VERSION_LOOKUP = {
    cv.Version(3, 3, 8): cv.Version(55, 3, 38, "1"),
    cv.Version(3, 3, 7): cv.Version(55, 3, 37),
    cv.Version(3, 3, 6): cv.Version(55, 3, 36),
    cv.Version(3, 3, 5): cv.Version(55, 3, 35),
    cv.Version(3, 3, 4): cv.Version(55, 3, 31, "2"),
    cv.Version(3, 3, 3): cv.Version(55, 3, 31, "2"),
    cv.Version(3, 3, 2): cv.Version(55, 3, 31, "2"),
    cv.Version(3, 3, 1): cv.Version(55, 3, 31, "2"),
    cv.Version(3, 3, 0): cv.Version(55, 3, 30, "2"),
    cv.Version(3, 2, 1): cv.Version(54, 3, 21, "2"),
    cv.Version(3, 2, 0): cv.Version(54, 3, 20),
    cv.Version(3, 1, 3): cv.Version(53, 3, 13),
    cv.Version(3, 1, 2): cv.Version(53, 3, 12),
    cv.Version(3, 1, 1): cv.Version(53, 3, 11),
    cv.Version(3, 1, 0): cv.Version(53, 3, 10),
}
# Maps Arduino framework versions to a compatible ESP-IDF version
# These versions correspond to pioarduino/esp-idf releases
# See: https://github.com/pioarduino/esp-idf/releases
ARDUINO_IDF_VERSION_LOOKUP = {
    cv.Version(3, 3, 8): cv.Version(5, 5, 4),
    cv.Version(3, 3, 7): cv.Version(5, 5, 3, "1"),
    cv.Version(3, 3, 6): cv.Version(5, 5, 2),
    cv.Version(3, 3, 5): cv.Version(5, 5, 2),
    cv.Version(3, 3, 4): cv.Version(5, 5, 1),
    cv.Version(3, 3, 3): cv.Version(5, 5, 1),
    cv.Version(3, 3, 2): cv.Version(5, 5, 1),
    cv.Version(3, 3, 1): cv.Version(5, 5, 1),
    cv.Version(3, 3, 0): cv.Version(5, 5, 0),
    cv.Version(3, 2, 1): cv.Version(5, 4, 2),
    cv.Version(3, 2, 0): cv.Version(5, 4, 2),
    cv.Version(3, 1, 3): cv.Version(5, 3, 2),
    cv.Version(3, 1, 2): cv.Version(5, 3, 2),
    cv.Version(3, 1, 1): cv.Version(5, 3, 1),
    cv.Version(3, 1, 0): cv.Version(5, 3, 0),
}

# The default/recommended esp-idf framework version
#  - https://github.com/espressif/esp-idf/releases
ESP_IDF_FRAMEWORK_VERSION_LOOKUP = {
    "recommended": cv.Version(5, 5, 4),
    "latest": cv.Version(5, 5, 4),
    "dev": cv.Version(5, 5, 4),
}

ESP_IDF_PLATFORM_VERSION_LOOKUP = {
    cv.Version(
        6, 0, 1
    ): "https://github.com/pioarduino/platform-espressif32.git#prep_IDF6",
    cv.Version(
        6, 0, 0
    ): "https://github.com/pioarduino/platform-espressif32.git#prep_IDF6",
    cv.Version(5, 5, 4): cv.Version(55, 3, 38, "1"),
    cv.Version(5, 5, 3, "1"): cv.Version(55, 3, 37),
    cv.Version(5, 5, 3): cv.Version(55, 3, 37),
    cv.Version(5, 5, 2): cv.Version(55, 3, 37),
    cv.Version(5, 5, 1): cv.Version(55, 3, 31, "2"),
    cv.Version(5, 5, 0): cv.Version(55, 3, 31, "2"),
    cv.Version(5, 4, 3): cv.Version(55, 3, 32),
    cv.Version(5, 4, 2): cv.Version(54, 3, 21, "2"),
    cv.Version(5, 4, 1): cv.Version(54, 3, 21, "2"),
    cv.Version(5, 4, 0): cv.Version(54, 3, 21, "2"),
    cv.Version(5, 3, 2): cv.Version(53, 3, 13),
    cv.Version(5, 3, 1): cv.Version(53, 3, 13),
    cv.Version(5, 3, 0): cv.Version(53, 3, 13),
    cv.Version(5, 1, 6): cv.Version(51, 3, 7),
    cv.Version(5, 1, 5): cv.Version(51, 3, 7),
}

# The platform-espressif32 version
#  - https://github.com/pioarduino/platform-espressif32/releases
PLATFORM_VERSION_LOOKUP = {
    "recommended": cv.Version(55, 3, 38, "1"),
    "latest": cv.Version(55, 3, 38, "1"),
    "dev": "https://github.com/pioarduino/platform-espressif32.git#develop",
}


def _check_pio_versions(config):
    config = config.copy()
    value = config[CONF_FRAMEWORK]

    if value[CONF_VERSION] in PLATFORM_VERSION_LOOKUP:
        if CONF_SOURCE in value or CONF_PLATFORM_VERSION in value:
            raise cv.Invalid(
                "Version needs to be explicitly set when a custom source or platform_version is used."
            )

        platform_lookup = PLATFORM_VERSION_LOOKUP[value[CONF_VERSION]]
        value[CONF_PLATFORM_VERSION] = _parse_pio_platform_version(str(platform_lookup))

        if value[CONF_TYPE] == FRAMEWORK_ARDUINO:
            version = ARDUINO_FRAMEWORK_VERSION_LOOKUP[value[CONF_VERSION]]
        else:
            version = ESP_IDF_FRAMEWORK_VERSION_LOOKUP[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))

    value[CONF_VERSION] = str(version)

    if value[CONF_TYPE] == FRAMEWORK_ARDUINO:
        if version < cv.Version(3, 0, 0):
            raise cv.Invalid("Only Arduino 3.0+ is supported.")
        recommended_version = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]
        platform_lookup = ARDUINO_PLATFORM_VERSION_LOOKUP.get(version)
        value[CONF_SOURCE] = value.get(
            CONF_SOURCE, _format_framework_arduino_version(version)
        )
        if _is_framework_url(value[CONF_SOURCE]):
            value[CONF_SOURCE] = f"{ARDUINO_FRAMEWORK_PKG}@{value[CONF_SOURCE]}"
    else:
        if version < cv.Version(5, 0, 0):
            raise cv.Invalid("Only ESP-IDF 5.0+ is supported.")
        recommended_version = ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]
        platform_lookup = ESP_IDF_PLATFORM_VERSION_LOOKUP.get(version)
        value[CONF_SOURCE] = value.get(
            CONF_SOURCE,
            _format_framework_pio_espidf_version(version, value.get(CONF_RELEASE)),
        )
        if _is_framework_url(value[CONF_SOURCE]):
            value[CONF_SOURCE] = f"pioarduino/framework-espidf@{value[CONF_SOURCE]}"

    if CONF_PLATFORM_VERSION not in value:
        if platform_lookup is None:
            raise cv.Invalid(
                "Framework version not recognized; please specify platform_version"
            )
        value[CONF_PLATFORM_VERSION] = _parse_pio_platform_version(str(platform_lookup))

    if version != recommended_version:
        _LOGGER.warning(
            "The selected framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    if value[CONF_PLATFORM_VERSION] != _parse_pio_platform_version(
        str(PLATFORM_VERSION_LOOKUP["recommended"])
    ):
        _LOGGER.warning(
            "The selected platform version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return config


def _check_esp_idf_versions(config):
    config = _check_pio_versions(config)
    value = config[CONF_FRAMEWORK]

    # Remove unwanted keys if present
    for key in (CONF_SOURCE, CONF_PLATFORM_VERSION):
        value.pop(key, None)

    # Official ESP-IDF frameworks don't use extra
    version = cv.Version.parse(value[CONF_VERSION])
    version = cv.Version(version.major, version.minor, version.patch)

    value[CONF_VERSION] = str(version)

    return config


def _validate_toolchain(value) -> Toolchain:
    return Toolchain(cv.one_of(*(t.value for t in Toolchain), lower=True)(value))


def _check_versions(config):
    # Resolve toolchain: CLI (already on CORE.toolchain) > YAML > default.
    if CORE.toolchain is None:
        CORE.toolchain = config.get(CONF_TOOLCHAIN, Toolchain.PLATFORMIO)

    if CORE.using_toolchain_esp_idf:
        return _check_esp_idf_versions(config)
    return _check_pio_versions(config)


def _parse_pio_platform_version(value):
    try:
        ver = cv.Version.parse(cv.version_number(value))
        release = f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}"
        if ver.extra:
            release += f"-{ver.extra}"
        return f"https://github.com/pioarduino/platform-espressif32/releases/download/{release}/platform-espressif32.zip"
    except cv.Invalid:
        return value


def _detect_variant(value):
    board = value.get(CONF_BOARD)
    variant = value.get(CONF_VARIANT)
    if variant and board is None:
        # If variant is set, we can derive the board from it
        # variant has already been validated against the known set
        value = value.copy()
        value[CONF_BOARD] = STANDARD_BOARDS[variant]
        if variant == VARIANT_ESP32P4:
            engineering_sample = value.get(CONF_ENGINEERING_SAMPLE)
            if engineering_sample is None:
                _LOGGER.warning(
                    "No board specified for ESP32-P4. Defaulting to production silicon (rev3).\n"
                    "If you have an early engineering sample (pre-rev3), add this to your config:\n"
                    "\n"
                    "  esp32:\n"
                    "    engineering_sample: true\n"
                    "\n"
                    "To check your chip revision, look for 'chip revision: vX.Y' in the boot log.\n"
                    "Engineering samples will show a revision below v3.0.\n"
                    "The 'debug:' component also reports the revision (e.g. Revision: 100 = v1.0, 300 = v3.0)."
                )
            elif engineering_sample:
                value[CONF_BOARD] = "esp32-p4-evboard"
    elif board in BOARDS:
        variant = variant or BOARDS[board][KEY_VARIANT]
        if variant != BOARDS[board][KEY_VARIANT]:
            raise cv.Invalid(
                f"Option '{CONF_VARIANT}' does not match selected board.",
                path=[CONF_VARIANT],
            )
        value = value.copy()
        value[CONF_VARIANT] = variant
    elif not variant:
        raise cv.Invalid(
            "This board is unknown, if you are sure you want to compile with this board selection, "
            f"override with option '{CONF_VARIANT}'",
            path=[CONF_BOARD],
        )
    else:
        _LOGGER.warning(
            "This board is unknown; the specified variant '%s' will be used but this may not work as expected.",
            variant,
        )
    return value


def final_validate(config):
    # Imported locally to avoid circular import issues
    from esphome.components.psram import DOMAIN as PSRAM_DOMAIN

    errs = []
    conf_fw = config[CONF_FRAMEWORK]
    advanced = conf_fw[CONF_ADVANCED]
    full_config = fv.full_config.get()
    if pio_options := full_config[CONF_ESPHOME].get(CONF_PLATFORMIO_OPTIONS):
        pio_flash_size_key = "board_upload.flash_size"
        pio_partitions_key = "board_build.partitions"
        if CONF_PARTITIONS in config and pio_partitions_key in pio_options:
            errs.append(
                cv.Invalid(
                    f"Do not specify '{pio_partitions_key}' in '{CONF_PLATFORMIO_OPTIONS}' with '{CONF_PARTITIONS}' in esp32"
                )
            )
        if pio_flash_size_key in pio_options:
            errs.append(
                cv.Invalid(
                    f"Please specify {CONF_FLASH_SIZE} within esp32 configuration only"
                )
            )
    if config[CONF_VARIANT] != VARIANT_ESP32 and advanced[CONF_IGNORE_EFUSE_MAC_CRC]:
        errs.append(
            cv.Invalid(
                f"'{CONF_IGNORE_EFUSE_MAC_CRC}' is not supported on {config[CONF_VARIANT]}",
                path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_IGNORE_EFUSE_MAC_CRC],
            )
        )
    if (
        config[CONF_VARIANT] != VARIANT_ESP32
        and advanced.get(CONF_MINIMUM_CHIP_REVISION) is not None
    ):
        errs.append(
            cv.Invalid(
                f"'{CONF_MINIMUM_CHIP_REVISION}' is only supported on {VARIANT_ESP32}",
                path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_MINIMUM_CHIP_REVISION],
            )
        )
    if config[CONF_VARIANT] != VARIANT_ESP32 and advanced[CONF_SRAM1_AS_IRAM]:
        errs.append(
            cv.Invalid(
                f"'{CONF_SRAM1_AS_IRAM}' is only supported on {VARIANT_ESP32}",
                path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_SRAM1_AS_IRAM],
            )
        )
    if (
        config[CONF_VARIANT] != VARIANT_ESP32P4
        and config.get(CONF_ENGINEERING_SAMPLE) is not None
    ):
        errs.append(
            cv.Invalid(
                f"'{CONF_ENGINEERING_SAMPLE}' is only supported on {VARIANT_ESP32P4}",
                path=[CONF_ENGINEERING_SAMPLE],
            )
        )
    if (
        config[CONF_VARIANT] == VARIANT_ESP32P4
        and config.get(CONF_ENGINEERING_SAMPLE) is not None
    ):
        board_is_es = BOARDS.get(config[CONF_BOARD], {}).get(
            "engineering_sample", False
        )
        if config[CONF_ENGINEERING_SAMPLE] != board_is_es:
            errs.append(
                cv.Invalid(
                    f"'{CONF_ENGINEERING_SAMPLE}' does not match board '{config[CONF_BOARD]}'",
                    path=[CONF_ENGINEERING_SAMPLE],
                )
            )
    if advanced[CONF_EXECUTE_FROM_PSRAM]:
        if config[CONF_VARIANT] not in {VARIANT_ESP32S3, VARIANT_ESP32P4}:
            errs.append(
                cv.Invalid(
                    f"'{CONF_EXECUTE_FROM_PSRAM}' is not available on this esp32 variant",
                    path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_EXECUTE_FROM_PSRAM],
                )
            )
        if PSRAM_DOMAIN not in full_config:
            errs.append(
                cv.Invalid(
                    f"'{CONF_EXECUTE_FROM_PSRAM}' requires PSRAM to be configured",
                    path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_EXECUTE_FROM_PSRAM],
                )
            )

    if (
        config[CONF_FLASH_SIZE] == "32MB"
        and "ota" in full_config
        and not advanced[CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES]
    ):
        errs.append(
            cv.Invalid(
                f"OTA with 32MB flash requires '{CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES}' to be set in the '{CONF_ADVANCED}' section of the esp32 configuration",
                path=[CONF_FLASH_SIZE],
            )
        )
    if advanced[CONF_ENABLE_OTA_ROLLBACK]:
        # "disabled: false" means safe mode *is* enabled.
        safe_mode_config = full_config.get(CONF_SAFE_MODE, {CONF_DISABLED: True})
        safe_mode_enabled = not safe_mode_config[CONF_DISABLED]
        ota_enabled = CONF_OTA in full_config
        # Both need to be enabled for rollback to work
        if not (ota_enabled and safe_mode_enabled):
            # But only warn if ota is even possible
            if ota_enabled:
                _LOGGER.warning(
                    "OTA rollback requires safe_mode, disabling rollback support"
                )
            # disable the rollback feature anyway since it can't be used.
            advanced[CONF_ENABLE_OTA_ROLLBACK] = False
    if signed_ota := advanced.get(CONF_SIGNED_OTA_VERIFICATION):
        scheme = signed_ota[CONF_SIGNING_SCHEME]
        variant = config[CONF_VARIANT]
        min_rev = advanced.get(CONF_MINIMUM_CHIP_REVISION)
        scheme_path = [
            CONF_FRAMEWORK,
            CONF_ADVANCED,
            CONF_SIGNED_OTA_VERIFICATION,
            CONF_SIGNING_SCHEME,
        ]

        # V1 ECDSA is only available on the original ESP32
        if scheme == "ecdsa_v1" and variant not in SIGNED_OTA_V1_ECDSA_VARIANTS:
            errs.append(
                cv.Invalid(
                    f"Signing scheme 'ecdsa_v1' is only supported on "
                    f"{VARIANT_FRIENDLY[VARIANT_ESP32]}. "
                    f"Use 'rsa3072' or 'ecdsa256' instead.",
                    path=scheme_path,
                )
            )
        elif variant == VARIANT_ESP32:
            # On ESP32, V2 RSA requires minimum_chip_revision >= 3.0
            # Note: string comparison works here because cv.one_of constrains
            # min_rev to known ESP32_CHIP_REVISIONS values ("0.0".."3.1").
            if scheme == "rsa3072" and (min_rev is None or min_rev < "3.0"):
                errs.append(
                    cv.Invalid(
                        f"Signing scheme 'rsa3072' on {VARIANT_FRIENDLY[variant]} "
                        f"requires minimum_chip_revision: '3.0' or higher "
                        f"(Secure Boot V2 RSA needs chip revision 3.0+). "
                        f"For older chip revisions, use 'ecdsa_v1' instead.",
                        path=scheme_path,
                    )
                )
            # ESP32 does not support V2 ECDSA (no SOC_SECURE_BOOT_V2_ECC)
            elif scheme == "ecdsa256":
                errs.append(
                    cv.Invalid(
                        f"Signing scheme 'ecdsa256' is not supported on "
                        f"{VARIANT_FRIENDLY[variant]}. Use 'rsa3072' (with "
                        f"minimum_chip_revision: '3.0') or 'ecdsa_v1' instead.",
                        path=scheme_path,
                    )
                )
            # V1 on rev 3.0+ -- suggest V2 RSA for stronger security
            elif scheme == "ecdsa_v1" and min_rev is not None and min_rev >= "3.0":
                _LOGGER.info(
                    "Using Secure Boot V1 ECDSA on %s rev %s. "
                    "Consider using 'rsa3072' (Secure Boot V2 RSA) for "
                    "stronger security on chip revision 3.0+.",
                    VARIANT_FRIENDLY[variant],
                    min_rev,
                )
        else:
            # Non-ESP32 variants: check V2 scheme-variant compatibility
            scheme_variant_conflicts = {
                "ecdsa256": (SIGNED_OTA_V2_RSA_ONLY_VARIANTS, "rsa3072"),
                "rsa3072": (SIGNED_OTA_V2_ECC_ONLY_VARIANTS, "ecdsa256"),
            }
            if (
                conflict := scheme_variant_conflicts.get(scheme)
            ) and variant in conflict[0]:
                errs.append(
                    cv.Invalid(
                        f"Signing scheme '{scheme}' is not supported on "
                        f"{VARIANT_FRIENDLY[variant]}. Use '{conflict[1]}' instead.",
                        path=scheme_path,
                    )
                )
        if CONF_OTA not in full_config:
            _LOGGER.warning(
                "Signed OTA verification is enabled but no OTA component is configured. "
                "The initial firmware will be signed but OTA updates won't be possible "
                "until an OTA component is added."
            )
        if CONF_SIGNING_KEY in signed_ota:
            _LOGGER.info(
                "Signed OTA verification is enabled. Keep your signing key safe! "
                "If you lose the signing key, you will NOT be able to OTA update "
                "devices running firmware signed with this key. "
                "Without the key, you'll need to reflash via serial."
            )
        else:
            _LOGGER.info(
                "Signed OTA verification is configured with a public verification key. "
                "Binaries will NOT be signed automatically during build. "
                "You must sign them externally before flashing."
            )
    if errs:
        raise cv.MultipleInvalid(errs)

    return config


CONF_SDKCONFIG_OPTIONS = "sdkconfig_options"
CONF_ENABLE_LWIP_DHCP_SERVER = "enable_lwip_dhcp_server"
CONF_ENABLE_LWIP_MDNS_QUERIES = "enable_lwip_mdns_queries"
CONF_ENABLE_LWIP_BRIDGE_INTERFACE = "enable_lwip_bridge_interface"
CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING = "enable_lwip_tcpip_core_locking"
CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY = "enable_lwip_check_thread_safety"
CONF_DISABLE_LIBC_LOCKS_IN_IRAM = "disable_libc_locks_in_iram"
CONF_DISABLE_VFS_SUPPORT_TERMIOS = "disable_vfs_support_termios"
CONF_DISABLE_VFS_SUPPORT_SELECT = "disable_vfs_support_select"
CONF_DISABLE_VFS_SUPPORT_DIR = "disable_vfs_support_dir"
CONF_FREERTOS_IN_IRAM = "freertos_in_iram"
CONF_RINGBUF_IN_IRAM = "ringbuf_in_iram"
CONF_HEAP_IN_IRAM = "heap_in_iram"
CONF_LOOP_TASK_STACK_SIZE = "loop_task_stack_size"
CONF_USE_FULL_CERTIFICATE_BUNDLE = "use_full_certificate_bundle"
CONF_DISABLE_DEBUG_STUBS = "disable_debug_stubs"
CONF_DISABLE_OCD_AWARE = "disable_ocd_aware"
CONF_DISABLE_USB_SERIAL_JTAG_SECONDARY = "disable_usb_serial_jtag_secondary"
CONF_DISABLE_DEV_NULL_VFS = "disable_dev_null_vfs"
CONF_DISABLE_MBEDTLS_PEER_CERT = "disable_mbedtls_peer_cert"
CONF_DISABLE_MBEDTLS_PKCS7 = "disable_mbedtls_pkcs7"
CONF_DISABLE_REGI2C_IN_IRAM = "disable_regi2c_in_iram"
CONF_DISABLE_FATFS = "disable_fatfs"
CONF_ADC_ONESHOT_IN_IRAM = "adc_oneshot_in_iram"

# VFS requirement tracking
# Components that need VFS features can call require_vfs_*() functions
KEY_VFS_SELECT_REQUIRED = "vfs_select_required"
KEY_VFS_DIR_REQUIRED = "vfs_dir_required"
KEY_VFS_TERMIOS_REQUIRED = "vfs_termios_required"
# Feature requirement tracking - components can call require_* functions to re-enable
# These are stored in CORE.data[KEY_ESP32] dict
KEY_USB_SERIAL_JTAG_SECONDARY_REQUIRED = "usb_serial_jtag_secondary_required"
KEY_MBEDTLS_PEER_CERT_REQUIRED = "mbedtls_peer_cert_required"
KEY_MBEDTLS_PKCS7_REQUIRED = "mbedtls_pkcs7_required"
KEY_FATFS_REQUIRED = "fatfs_required"
KEY_MBEDTLS_SHA512_REQUIRED = "mbedtls_sha512_required"
KEY_ADC_ONESHOT_IRAM_REQUIRED = "adc_oneshot_iram_required"


def require_vfs_select() -> None:
    """Mark that VFS select support is required by a component.

    Call this from components that use esp_vfs_eventfd or other VFS select features.
    This prevents CONFIG_VFS_SUPPORT_SELECT from being disabled.
    """
    CORE.data[KEY_VFS_SELECT_REQUIRED] = True


def require_vfs_dir() -> None:
    """Mark that VFS directory support is required by a component.

    Call this from components that use directory functions (opendir, readdir, mkdir, etc.).
    This prevents CONFIG_VFS_SUPPORT_DIR from being disabled.
    """
    CORE.data[KEY_VFS_DIR_REQUIRED] = True


def require_vfs_termios() -> None:
    """Mark that VFS termios support is required by a component.

    Call this from components that use terminal I/O functions (usb_serial_jtag_vfs_*, etc.).
    This prevents CONFIG_VFS_SUPPORT_TERMIOS from being disabled.
    """
    CORE.data[KEY_VFS_TERMIOS_REQUIRED] = True


def require_full_certificate_bundle() -> None:
    """Request the full certificate bundle instead of the common-CAs-only bundle.

    By default, ESPHome uses CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN which
    includes only CAs with >1% market share (~51 KB smaller than full bundle).
    This covers ~99% of websites including Let's Encrypt, DigiCert, Google, Amazon.

    Call this from components that need to connect to services using uncommon CAs.
    """
    CORE.data[KEY_ESP32][KEY_FULL_CERT_BUNDLE] = True


def require_usb_serial_jtag_secondary() -> None:
    """Mark that USB Serial/JTAG secondary console is required by a component.

    Call this from components (e.g., logger) that need USB Serial/JTAG console output.
    This prevents CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG from being disabled.
    """
    CORE.data[KEY_ESP32][KEY_USB_SERIAL_JTAG_SECONDARY_REQUIRED] = True


def require_mbedtls_peer_cert() -> None:
    """Mark that mbedTLS peer certificate retention is required by a component.

    Call this from components that need access to the peer certificate after
    the TLS handshake is complete. This prevents CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE
    from being disabled.
    """
    CORE.data[KEY_ESP32][KEY_MBEDTLS_PEER_CERT_REQUIRED] = True


def require_mbedtls_pkcs7() -> None:
    """Mark that mbedTLS PKCS#7 support is required by a component.

    Call this from components that need PKCS#7 certificate validation.
    This prevents CONFIG_MBEDTLS_PKCS7_C from being disabled.
    """
    CORE.data[KEY_ESP32][KEY_MBEDTLS_PKCS7_REQUIRED] = True


def require_mbedtls_sha512() -> None:
    """Mark that mbedTLS SHA-384/SHA-512 support is required by a component.

    Call this from components that need to verify TLS certificates or signatures
    using SHA-384 or SHA-512 algorithms. This prevents CONFIG_MBEDTLS_SHA384_C
    and CONFIG_MBEDTLS_SHA512_C from being disabled.
    """
    CORE.data[KEY_ESP32][KEY_MBEDTLS_SHA512_REQUIRED] = True


def idf_version() -> cv.Version:
    """Return the underlying ESP-IDF version regardless of framework choice.

    For ESP-IDF builds this is the framework version directly.
    For Arduino builds this is the mapped IDF version from ARDUINO_IDF_VERSION_LOOKUP.
    """
    return CORE.data[KEY_ESP32][KEY_IDF_VERSION]


def require_fatfs() -> None:
    """Mark that FATFS support is required by a component.

    Call this from components that use FATFS (e.g., SD card, storage components).
    This prevents FATFS from being disabled when disable_fatfs is set.
    """
    CORE.data[KEY_ESP32][KEY_FATFS_REQUIRED] = True


def require_adc_oneshot_iram() -> None:
    """Mark that ADC oneshot IRAM safety is required by a component.

    Call this from components that use the ADC oneshot driver. When flash cache is
    disabled (e.g., during NVS writes by WiFi, BLE, Zigbee, or power management),
    the ADC oneshot read function must be in IRAM to avoid crashes.
    This sets CONFIG_ADC_ONESHOT_CTRL_FUNC_IN_IRAM.
    """
    CORE.data[KEY_ESP32][KEY_ADC_ONESHOT_IRAM_REQUIRED] = True


def _parse_idf_component(value: str) -> ConfigType:
    """Parse IDF component shorthand syntax like 'owner/component^version'"""
    # Match operator followed by version-like string (digit or *)
    if match := re.search(r"(~=|>=|<=|==|!=|>|<|\^|~)(\d|\*)", value):
        return {CONF_NAME: value[: match.start()], CONF_REF: value[match.start() :]}
    raise cv.Invalid(
        f"Invalid IDF component shorthand '{value}'. "
        f"Expected format: 'owner/component<op>version' where <op> is one of: ^, ~, ~=, ==, !=, >=, >, <=, <"
    )


FRAMEWORK_ESP_IDF = "esp-idf"
FRAMEWORK_ARDUINO = "arduino"
FRAMEWORK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TYPE): cv.one_of(FRAMEWORK_ESP_IDF, FRAMEWORK_ARDUINO),
        cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
        cv.Optional(CONF_RELEASE): cv.string_strict,
        cv.Optional(CONF_SOURCE): cv.string_strict,
        cv.Optional(CONF_PLATFORM_VERSION): _parse_pio_platform_version,
        cv.Optional(CONF_SDKCONFIG_OPTIONS, default={}): {
            cv.string_strict: cv.string_strict
        },
        cv.Optional(CONF_LOG_LEVEL, default="ERROR"): cv.one_of(
            *LOG_LEVELS_IDF, upper=True
        ),
        cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
            {
                cv.Optional(CONF_ASSERTION_LEVEL): cv.one_of(
                    *ASSERTION_LEVELS, upper=True
                ),
                cv.Optional(CONF_COMPILER_OPTIMIZATION, default="SIZE"): cv.one_of(
                    *COMPILER_OPTIMIZATIONS, upper=True
                ),
                cv.Optional(
                    CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES, default=False
                ): cv.boolean,
                cv.Optional(CONF_ENABLE_LWIP_ASSERT, default=True): cv.boolean,
                cv.Optional(CONF_IGNORE_EFUSE_CUSTOM_MAC, default=False): cv.boolean,
                cv.Optional(CONF_IGNORE_EFUSE_MAC_CRC, default=False): cv.boolean,
                cv.Optional(CONF_MINIMUM_CHIP_REVISION): cv.one_of(
                    *ESP32_CHIP_REVISIONS, string=True
                ),
                cv.Optional(CONF_SRAM1_AS_IRAM, default=False): cv.boolean,
                # DHCP server is needed for WiFi AP mode. When WiFi component is used,
                # it will handle disabling DHCP server when AP is not configured.
                # Default to false (disabled) when WiFi is not used.
                cv.OnlyWithout(
                    CONF_ENABLE_LWIP_DHCP_SERVER, "wifi", default=False
                ): cv.boolean,
                cv.Optional(CONF_ENABLE_LWIP_MDNS_QUERIES, default=True): cv.boolean,
                cv.Optional(
                    CONF_ENABLE_LWIP_BRIDGE_INTERFACE, default=False
                ): cv.boolean,
                cv.Optional(
                    CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING, default=True
                ): cv.boolean,
                cv.Optional(
                    CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY, default=True
                ): cv.boolean,
                cv.Optional(CONF_DISABLE_LIBC_LOCKS_IN_IRAM, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_VFS_SUPPORT_TERMIOS, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_VFS_SUPPORT_SELECT, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_VFS_SUPPORT_DIR, default=True): cv.boolean,
                cv.Optional(CONF_FREERTOS_IN_IRAM, default=False): cv.boolean,
                cv.Optional(CONF_RINGBUF_IN_IRAM, default=False): cv.boolean,
                cv.Optional(CONF_HEAP_IN_IRAM, default=False): cv.boolean,
                cv.Optional(CONF_EXECUTE_FROM_PSRAM, default=False): cv.boolean,
                cv.Optional(CONF_LOOP_TASK_STACK_SIZE, default=8192): cv.int_range(
                    min=8192, max=32768
                ),
                cv.Optional(CONF_ENABLE_OTA_ROLLBACK, default=True): cv.boolean,
                cv.Optional(CONF_SIGNED_OTA_VERIFICATION): cv.All(
                    cv.Schema(
                        {
                            cv.Optional(CONF_SIGNING_KEY): cv.file_,
                            cv.Optional(CONF_VERIFICATION_KEY): cv.file_,
                            cv.Optional(
                                CONF_SIGNING_SCHEME, default="rsa3072"
                            ): cv.one_of(*SIGNING_SCHEMES, lower=True),
                        }
                    ),
                    cv.has_exactly_one_key(CONF_SIGNING_KEY, CONF_VERIFICATION_KEY),
                ),
                cv.Optional(
                    CONF_USE_FULL_CERTIFICATE_BUNDLE, default=False
                ): cv.boolean,
                cv.Optional(
                    CONF_INCLUDE_BUILTIN_IDF_COMPONENTS, default=[]
                ): cv.ensure_list(cv.string_strict),
                cv.Optional(CONF_ENABLE_FULL_PRINTF, default=False): cv.boolean,
                cv.Optional(CONF_DISABLE_DEBUG_STUBS, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_OCD_AWARE, default=True): cv.boolean,
                cv.Optional(
                    CONF_DISABLE_USB_SERIAL_JTAG_SECONDARY, default=True
                ): cv.boolean,
                cv.Optional(CONF_DISABLE_DEV_NULL_VFS, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_MBEDTLS_PEER_CERT, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_MBEDTLS_PKCS7, default=True): cv.boolean,
                cv.Optional(CONF_DISABLE_REGI2C_IN_IRAM, default=True): cv.boolean,
                cv.Optional(CONF_ADC_ONESHOT_IN_IRAM, default=False): cv.boolean,
                cv.Optional(CONF_DISABLE_FATFS, default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_COMPONENTS, default=[]): cv.ensure_list(
            cv.All(
                cv.Any(
                    cv.All(cv.string_strict, _parse_idf_component),
                    cv.Schema(
                        {
                            cv.Required(CONF_NAME): cv.string_strict,
                            cv.Optional(CONF_SOURCE): cv.git_ref,
                            cv.Optional(CONF_REF): cv.string,
                            cv.Optional(CONF_PATH): cv.string,
                        }
                    ),
                ),
            )
        ),
    }
)


# Remove this class in 2026.7.0
class _FrameworkMigrationWarning:
    shown = False


def _show_framework_migration_message(name: str, variant: str) -> None:
    """Show a message about the framework default change and how to switch back to Arduino."""
    # Remove this function in 2026.7.0
    if _FrameworkMigrationWarning.shown:
        return
    _FrameworkMigrationWarning.shown = True

    from esphome.log import AnsiFore, color

    message = (
        color(
            AnsiFore.BOLD_CYAN,
            f"💡 NOTICE: {name} does not have a framework specified.",
        )
        + "\n\n"
        + f"Starting with ESPHome 2026.1.0, the default framework for {variant} is ESP-IDF.\n"
        + "(We've been warning about this change since ESPHome 2025.8.0)\n"
        + "\n"
        + "Why we made this change:\n"
        + color(AnsiFore.GREEN, "  ✨ Smaller firmware binaries\n")
        + color(AnsiFore.GREEN, "  ⚡ Faster compile times\n")
        + color(AnsiFore.GREEN, "  🚀 Better performance and newer features\n")
        + color(AnsiFore.GREEN, "  🔧 More actively maintained by ESPHome\n")
        + "\n"
        + "To continue using Arduino, add this to your YAML under 'esp32:':\n"
        + color(AnsiFore.WHITE, "    framework:\n")
        + color(AnsiFore.WHITE, "      type: arduino\n")
        + "\n"
        + "To silence this message with ESP-IDF, explicitly set:\n"
        + color(AnsiFore.WHITE, "    framework:\n")
        + color(AnsiFore.WHITE, "      type: esp-idf\n")
        + "\n"
        + "Migration guide: "
        + color(
            AnsiFore.BLUE,
            "https://esphome.io/guides/esp32_arduino_to_idf/",
        )
    )
    _LOGGER.warning(message)


def _set_default_framework(config):
    config = config.copy()
    if CONF_FRAMEWORK not in config:
        config[CONF_FRAMEWORK] = FRAMEWORK_SCHEMA({})
    if CONF_TYPE not in config[CONF_FRAMEWORK]:
        variant = config[CONF_VARIANT]
        config[CONF_FRAMEWORK][CONF_TYPE] = FRAMEWORK_ESP_IDF
        # Show migration message for variants that previously defaulted to Arduino
        # Remove this message in 2026.7.0
        if variant in ARDUINO_ALLOWED_VARIANTS:
            _show_framework_migration_message(
                config.get(CONF_NAME, "This device"), variant
            )

    return config


RESERVED_PARTITION_NAMES = {
    "nvs",
    "app0",
    "app1",
    "otadata",
    "eeprom",
    "spiffs",
    "phy_init",
}

VALID_APP_SUBTYPES = {"factory", "test"}
VALID_DATA_SUBTYPES = {
    "nvs",
    "nvs_keys",
    "spiffs",
    "coredump",
    "efuse",
    "fat",
    "undefined",
    "littlefs",
}


def _validate_custom_partition(config: ConfigType) -> ConfigType:
    """Voluptuous validator for custom partition schema."""
    try:
        _validate_partition(
            config[CONF_NAME],
            config[CONF_TYPE],
            config[CONF_SUBTYPE],
            config[CONF_SIZE],
        )
    except ValueError as e:
        raise cv.Invalid(str(e)) from e
    return config


FLASH_SIZES = [
    "2MB",
    "4MB",
    "8MB",
    "16MB",
    "32MB",
]

CONF_FLASH_SIZE = "flash_size"
CONF_CPU_FREQUENCY = "cpu_frequency"
CONF_PARTITIONS = "partitions"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_BOARD): cv.All(
                cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
            ),
            cv.Optional(CONF_CPU_FREQUENCY): cv.one_of(
                *FULL_CPU_FREQUENCIES, upper=True
            ),
            cv.Optional(CONF_ENGINEERING_SAMPLE): cv.boolean,
            cv.Optional(CONF_FLASH_SIZE, default="4MB"): cv.one_of(
                *FLASH_SIZES, upper=True
            ),
            cv.Optional(CONF_PARTITIONS): cv.Any(
                cv.file_,
                cv.ensure_list(
                    cv.All(
                        cv.Schema(
                            {
                                cv.Required(CONF_NAME): cv.string_strict,
                                cv.Required(CONF_TYPE): cv.All(
                                    cv.Any(cv.string_strict, cv.int_range(0x40, 0xFE)),
                                    cv.int_to_hex_string,
                                ),
                                cv.Required(CONF_SUBTYPE): cv.All(
                                    cv.Any(cv.string_strict, cv.int_range(0, 0xFE)),
                                    cv.int_to_hex_string,
                                ),
                                cv.Required(CONF_SIZE): cv.int_range(min=0x1000),
                            }
                        ),
                        _validate_custom_partition,
                    ),
                ),
            ),
            cv.Optional(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
            cv.Optional(CONF_FRAMEWORK): FRAMEWORK_SCHEMA,
            cv.Optional(CONF_TOOLCHAIN): _validate_toolchain,
            cv.Optional(CONF_WATCHDOG_TIMEOUT, default="5s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(min=cv.TimePeriod(seconds=5), max=cv.TimePeriod(seconds=60)),
            ),
        }
    ),
    _detect_variant,
    _set_default_framework,
    _check_versions,
    set_core_data,
    cv.has_at_least_one_key(CONF_BOARD, CONF_VARIANT),
)


FINAL_VALIDATE_SCHEMA = cv.Schema(final_validate)


def _configure_lwip_max_sockets(conf: dict) -> None:
    """Calculate and set CONFIG_LWIP_MAX_SOCKETS based on component needs.

    Socket component tracks consumer needs via consume_sockets() called during config validation.
    This function runs in to_code() after all components have registered their socket needs.
    User-provided sdkconfig_options take precedence.
    """
    from esphome.components.socket import get_socket_counts

    # Check if user manually specified CONFIG_LWIP_MAX_SOCKETS
    user_max_sockets = conf[CONF_SDKCONFIG_OPTIONS].get("CONFIG_LWIP_MAX_SOCKETS")

    # CONFIG_LWIP_MAX_SOCKETS is a single VFS socket pool shared by all socket
    # types (TCP clients, TCP listeners, and UDP). Include all three counts.
    sc = get_socket_counts()
    total_sockets = sc.tcp + sc.udp + sc.tcp_listen

    # User specified their own value - respect it but warn if insufficient
    if user_max_sockets is not None:
        _LOGGER.info(
            "Using user-provided CONFIG_LWIP_MAX_SOCKETS: %s",
            user_max_sockets,
        )

        user_sockets_int = 0
        with contextlib.suppress(ValueError, TypeError):
            user_sockets_int = int(user_max_sockets)

        if user_sockets_int < total_sockets:
            _LOGGER.warning(
                "CONFIG_LWIP_MAX_SOCKETS is set to %d but your configuration "
                "needs %d sockets (%d TCP + %d UDP + %d TCP_LISTEN). You may "
                "experience socket exhaustion errors. Consider increasing to "
                "at least %d.",
                user_sockets_int,
                total_sockets,
                sc.tcp,
                sc.udp,
                sc.tcp_listen,
                total_sockets,
            )
        # User's value already added via sdkconfig_options processing
        return

    # Auto-calculate based on component needs
    # Use at least the ESP-IDF default (10), or the total needed by components
    max_sockets = max(DEFAULT_MAX_SOCKETS, total_sockets)

    log_level = logging.INFO if max_sockets > DEFAULT_MAX_SOCKETS else logging.DEBUG
    sock_min = " (min)" if max_sockets > total_sockets else ""
    _LOGGER.log(
        log_level,
        "Setting CONFIG_LWIP_MAX_SOCKETS to %d%s "
        "(TCP=%d [%s], UDP=%d [%s], TCP_LISTEN=%d [%s])",
        max_sockets,
        sock_min,
        sc.tcp,
        sc.tcp_details,
        sc.udp,
        sc.udp_details,
        sc.tcp_listen,
        sc.tcp_listen_details,
    )

    add_idf_sdkconfig_option("CONFIG_LWIP_MAX_SOCKETS", max_sockets)


@coroutine_with_priority(CoroPriority.FINAL)
async def _write_exclude_components() -> None:
    """Write EXCLUDE_COMPONENTS cmake arg after all components have registered exclusions."""
    if KEY_ESP32 not in CORE.data:
        return
    excluded = CORE.data[KEY_ESP32].get(KEY_EXCLUDE_COMPONENTS)
    if excluded:
        exclude_list = ";".join(sorted(excluded))
        cg.add_platformio_option(
            "board_build.cmake_extra_args", f"-DEXCLUDE_COMPONENTS={exclude_list}"
        )


@coroutine_with_priority(CoroPriority.FINAL)
async def _write_arduino_libs_stub(stubs_dir: Path, idf_ver: cv.Version) -> None:
    """Write stub package to skip downloading precompiled Arduino libs."""
    stubs_dir.mkdir(parents=True, exist_ok=True)
    write_file_if_changed(
        stubs_dir / "package.json",
        f'{{"name":"{ARDUINO_LIBS_NAME}","version":"{idf_ver.major}.{idf_ver.minor}.{idf_ver.patch}"}}',
    )
    write_file_if_changed(
        stubs_dir / "tools.json",
        '{"packages":[{"platforms":[{"toolsDependencies":[]}],"tools":[]}]}',
    )


@coroutine_with_priority(CoroPriority.FINAL)
async def _write_arduino_libraries_sdkconfig() -> None:
    """Write Arduino selective compilation sdkconfig after all components have added libraries.

    This must run at FINAL priority so that all components have had a chance to call
    cg.add_library() which auto-enables Arduino libraries via _enable_arduino_library().
    """
    if KEY_ESP32 not in CORE.data:
        return
    # Enable Arduino selective compilation to disable unused Arduino libraries
    # ESPHome uses ESP-IDF APIs directly; we only need the Arduino core
    # (HardwareSerial, Print, Stream, GPIO functions which are always compiled)
    # cg.add_library() auto-enables needed libraries; users can also add
    # libraries via esphome: libraries: config which calls cg.add_library()
    add_idf_sdkconfig_option("CONFIG_ARDUINO_SELECTIVE_COMPILATION", True)
    enabled_libs = CORE.data[KEY_ESP32].get(KEY_ARDUINO_LIBRARIES, set())
    for lib in ARDUINO_DISABLED_LIBRARIES:
        # Enable if explicitly requested, disable otherwise
        add_idf_sdkconfig_option(f"CONFIG_ARDUINO_SELECTIVE_{lib}", lib in enabled_libs)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_yaml_idf_components(components: list[ConfigType]):
    """Add IDF components from YAML config with final priority to override code-added components."""
    for component in components:
        add_idf_component(
            name=component[CONF_NAME],
            repo=component.get(CONF_SOURCE),
            ref=component.get(CONF_REF),
            path=component.get(CONF_PATH),
        )


async def to_code(config):
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    conf = config[CONF_FRAMEWORK]

    # Check if using ESP-IDF toolchain
    use_platformio = not CORE.using_toolchain_esp_idf
    if use_platformio:
        # Clear IDF environment variables to avoid conflicts with PlatformIO's ESP-IDF
        # but keep them when using ESP-IDF toolchain
        for clean_var in ("IDF_PATH", "IDF_TOOLS_PATH"):
            os.environ.pop(clean_var, None)

        cg.add_platformio_option("lib_ldf_mode", "off")
        cg.add_platformio_option("lib_compat_mode", "strict")
        cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
        cg.add_platformio_option("board", config[CONF_BOARD])
        cg.add_platformio_option("board_upload.flash_size", config[CONF_FLASH_SIZE])
        cg.add_platformio_option(
            "board_upload.maximum_size",
            int(config[CONF_FLASH_SIZE].removesuffix("MB")) * 1024 * 1024,
        )

        if CONF_SOURCE in conf:
            cg.add_platformio_option("platform_packages", [conf[CONF_SOURCE]])

        add_extra_script(
            "pre",
            "pre_build.py",
            Path(__file__).parent / "pre_build.py.script",
        )

        add_extra_script(
            "post",
            "post_build.py",
            Path(__file__).parent / "post_build.py.script",
        )

        # In testing mode, add IRAM fix script to allow linking grouped component tests
        # Similar to ESP8266's approach but for ESP-IDF
        if CORE.testing_mode:
            cg.add_build_flag("-DESPHOME_TESTING_MODE")
            add_extra_script(
                "pre",
                "iram_fix.py",
                Path(__file__).parent / "iram_fix.py.script",
            )
    else:
        cg.add_build_flag("-Wno-error=format")
        cg.add_build_flag("-Wno-error=missing-field-initializers")
        cg.add_build_flag("-Wno-error=volatile")

    cg.set_cpp_standard("gnu++20")
    cg.add_build_flag("-DUSE_ESP32")
    cg.add_define("USE_NATIVE_64BIT_TIME")
    cg.add_build_flag("-Wl,-z,noexecstack")
    # Arduino already wraps esp_panic_handler for its own backtrace handler,
    # so only add our wrap when using ESP-IDF framework to avoid linker conflicts.
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_build_flag("-Wl,--wrap=esp_panic_handler")
        cg.add_define("USE_ESP32_CRASH_HANDLER")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    variant = config[CONF_VARIANT]
    cg.add_build_flag(f"-DUSE_ESP32_VARIANT_{variant}")
    cg.add_define("ESPHOME_VARIANT", VARIANT_FRIENDLY[variant])
    cg.add_define(ThreadModel.MULTI_ATOMICS)

    if conf[CONF_ADVANCED][CONF_IGNORE_EFUSE_CUSTOM_MAC]:
        cg.add_define("USE_ESP32_IGNORE_EFUSE_CUSTOM_MAC")

    # Set the location of the IDF component manager cache
    os.environ["IDF_COMPONENT_CACHE_PATH"] = str(
        CORE.relative_internal_path(".espressif")
    )

    # Both ESP-IDF and ESP32 Arduino builds generate IDF app metadata. Keep
    # volatile build path/time data out of the binary so equivalent projects can
    # produce reproducible outputs and downstream tooling can reuse artifacts.
    add_idf_sdkconfig_option("CONFIG_APP_REPRODUCIBLE_BUILD", True)

    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        if use_platformio:
            cg.add_platformio_option("framework", "espidf")

        # Wrap std::__throw_* functions to abort immediately, eliminating ~3KB of
        # exception class overhead. See throw_stubs.cpp for implementation.
        # ESP-IDF already compiles with -fno-exceptions, so this code was dead anyway.
        for mangled in [
            "_ZSt20__throw_length_errorPKc",
            "_ZSt19__throw_logic_errorPKc",
            "_ZSt20__throw_out_of_rangePKc",
            "_ZSt24__throw_out_of_range_fmtPKcz",
            "_ZSt17__throw_bad_allocv",
            "_ZSt25__throw_bad_function_callv",
        ]:
            cg.add_build_flag(f"-Wl,--wrap={mangled}")

        # Wrap FILE*-based printf functions to eliminate newlib's _vfprintf_r
        # (~11 KB). See printf_stubs.cpp for implementation.
        #
        # The wrap is only beneficial against newlib. Picolibc's tinystdio
        # implements vsnprintf by building a string-output FILE and calling
        # vfprintf, so vfprintf is unconditionally linked in by any caller
        # of snprintf/vsnprintf — effectively every build — and the wrap
        # saves nothing while costing ~170 B of shim. IDF 5.x defaults to
        # newlib on every variant; IDF 6.0+ switches to picolibc on every
        # variant.
        if conf[CONF_ADVANCED][CONF_ENABLE_FULL_PRINTF] or idf_version() >= cv.Version(
            6, 0, 0
        ):
            cg.add_define("USE_FULL_PRINTF")
        else:
            for symbol in ("vprintf", "printf", "fprintf", "vfprintf"):
                cg.add_build_flag(f"-Wl,--wrap={symbol}")
    else:
        cg.add_build_flag("-DUSE_ARDUINO")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ARDUINO")
        if use_platformio:
            cg.add_platformio_option("framework", "arduino, espidf")

            # Add IDF framework source for Arduino builds to ensure it uses the same version as
            # the ESP-IDF framework
            if (idf_ver := ARDUINO_IDF_VERSION_LOOKUP.get(framework_ver)) is not None:
                cg.add_platformio_option(
                    "platform_packages",
                    [_format_framework_pio_espidf_version(idf_ver)],
                )
                # Use stub package to skip downloading precompiled libs
                stubs_dir = CORE.relative_build_path("arduino_libs_stub")
                cg.add_platformio_option(
                    "platform_packages", [f"{ARDUINO_LIBS_PKG}@file://{stubs_dir}"]
                )
                CORE.add_job(_write_arduino_libs_stub, stubs_dir, idf_ver)

            # ESP32-S2 Arduino: Disable USB Serial on boot to avoid TinyUSB dependency
            if get_esp32_variant() == VARIANT_ESP32S2:
                cg.add_build_unflag("-DARDUINO_USB_CDC_ON_BOOT=1")
                cg.add_build_unflag("-DARDUINO_USB_CDC_ON_BOOT=0")
                cg.add_build_flag("-DARDUINO_USB_CDC_ON_BOOT=0")

        cg.add_define(
            "USE_ARDUINO_VERSION_CODE",
            cg.RawExpression(
                f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
            ),
        )

        add_idf_sdkconfig_option("CONFIG_MBEDTLS_PSK_MODES", True)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE", True)

    cg.add_build_flag("-Wno-nonnull-compare")

    # Use CMN (common CAs) bundle by default to save ~51KB flash
    # CMN covers CAs with >1% market share (~99% of websites)
    # Components needing uncommon CAs can call require_full_certificate_bundle()
    use_full_bundle = conf[CONF_ADVANCED].get(
        CONF_USE_FULL_CERTIFICATE_BUNDLE, False
    ) or CORE.data[KEY_ESP32].get(KEY_FULL_CERT_BUNDLE, False)
    add_idf_sdkconfig_option(
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL", use_full_bundle
    )
    if not use_full_bundle:
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN", True)

    add_idf_sdkconfig_option(f"CONFIG_IDF_TARGET_{variant}", True)
    add_idf_sdkconfig_option(
        f"CONFIG_ESPTOOLPY_FLASHSIZE_{config[CONF_FLASH_SIZE]}", True
    )

    # ESP32-P4: ESP-IDF 5.5.3 changed the default of ESP32P4_SELECTS_REV_LESS_V3
    # from y to n. PlatformIO uses sections.ld.in (for rev <3) or
    # sections.rev3.ld.in (for rev >=3) based on board definition.
    # Set the sdkconfig option to match the board's chip revision.
    if variant == VARIANT_ESP32P4:
        is_eng_sample = BOARDS.get(config[CONF_BOARD], {}).get(
            "engineering_sample", False
        )
        add_idf_sdkconfig_option("CONFIG_ESP32P4_SELECTS_REV_LESS_V3", is_eng_sample)

    # Set minimum chip revision for ESP32 variant
    # Setting this to 3.0 or higher reduces flash size by excluding workaround code,
    # and for PSRAM users saves significant IRAM by keeping C library functions in ROM.
    if variant == VARIANT_ESP32:
        min_rev = conf[CONF_ADVANCED].get(CONF_MINIMUM_CHIP_REVISION)
        if min_rev is not None:
            for rev, flag in ESP32_CHIP_REVISIONS.items():
                add_idf_sdkconfig_option(flag, rev == min_rev)
            cg.add_define("USE_ESP32_MIN_CHIP_REVISION_SET")

    # Use SRAM1 region as IRAM on ESP32 (original) variant
    # This provides an additional 40KB of IRAM by using SRAM1 memory that was previously
    # reserved for bootloader DRAM. Requires a bootloader from ESP-IDF v5.1 or later.
    # WARNING: If the device has an old bootloader (pre-v5.1), the app will fail to boot.
    # A USB flash will update the bootloader automatically. OTA updates do not.
    # See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/ram-usage.html
    if variant == VARIANT_ESP32 and conf[CONF_ADVANCED][CONF_SRAM1_AS_IRAM]:
        add_idf_sdkconfig_option("CONFIG_ESP_SYSTEM_ESP32_SRAM1_REGION_AS_IRAM", True)
        cg.add_define("USE_ESP32_SRAM1_AS_IRAM")
    add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_SINGLE_APP", False)
    add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_CUSTOM", True)
    add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_CUSTOM_FILENAME", "partitions.csv")

    # Increase freertos tick speed from 100Hz to 1kHz so that delay() resolution is 1ms
    add_idf_sdkconfig_option("CONFIG_FREERTOS_HZ", 1000)

    # Place non-ISR FreeRTOS functions into flash instead of IRAM
    # This saves up to 8KB of IRAM. ISR-safe functions (FromISR variants) stay in IRAM.
    # In ESP-IDF 6.0 this becomes the default and CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH
    # is removed (replaced by CONFIG_FREERTOS_IN_IRAM to restore old behavior).
    # We enable this now to match IDF 6.0 behavior and catch any issues early.
    # Users can set freertos_in_iram: true as an escape hatch if they encounter problems
    # with code that incorrectly calls FreeRTOS functions from ISRs with cache disabled.
    if conf[CONF_ADVANCED][CONF_FREERTOS_IN_IRAM]:
        # IDF 5.x: don't set the flash option (keeps functions in IRAM)
        # IDF 6.0+: will need CONFIG_FREERTOS_IN_IRAM=y to restore IRAM placement
        add_idf_sdkconfig_option("CONFIG_FREERTOS_IN_IRAM", True)
    else:
        # IDF 5.x: explicitly place functions in flash
        # IDF 6.0+: this is the default, option no longer exists
        add_idf_sdkconfig_option("CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH", True)

    # Place ring buffer functions into flash instead of IRAM by default
    # This saves IRAM. In ESP-IDF 6.0 flash placement becomes the default.
    # Users can set ringbuf_in_iram: true as an escape hatch if they encounter issues.
    if conf[CONF_ADVANCED][CONF_RINGBUF_IN_IRAM]:
        # User requests ring buffer in IRAM
        # IDF 6.0+: will need CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH=n
        add_idf_sdkconfig_option("CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH", False)
    else:
        # Place in flash to save IRAM (default)
        add_idf_sdkconfig_option("CONFIG_RINGBUF_PLACE_FUNCTIONS_INTO_FLASH", True)

    # Place heap functions into flash to save IRAM (~4-6KB savings)
    # Safe as long as heap functions are not called from ISRs (which they shouldn't be)
    # Users can set heap_in_iram: true as an escape hatch if needed
    if not conf[CONF_ADVANCED][CONF_HEAP_IN_IRAM]:
        add_idf_sdkconfig_option("CONFIG_HEAP_PLACE_FUNCTION_INTO_FLASH", True)

    # Setup watchdog
    add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT", True)
    add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_PANIC", True)
    add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0", False)
    add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1", False)
    add_idf_sdkconfig_option(
        "CONFIG_ESP_TASK_WDT_TIMEOUT_S",
        config[CONF_WATCHDOG_TIMEOUT].total_seconds,
    )

    # Disable dynamic log level control to save memory
    add_idf_sdkconfig_option("CONFIG_LOG_DYNAMIC_LEVEL_CONTROL", False)

    # Disable per-tag log level filtering since dynamic level control is disabled above
    # This saves ~250 bytes of RAM (tag cache) and associated code
    add_idf_sdkconfig_option("CONFIG_LOG_TAG_LEVEL_IMPL_NONE", True)

    # Reduce PHY TX power in the event of a brownout
    add_idf_sdkconfig_option("CONFIG_ESP_PHY_REDUCE_TX_POWER", True)

    # Set default CPU frequency
    add_idf_sdkconfig_option(
        f"CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_{config[CONF_CPU_FREQUENCY][:-3]}", True
    )

    # Apply LWIP optimization settings
    advanced = conf[CONF_ADVANCED]

    # Re-include any IDF components the user explicitly requested
    for component_name in advanced.get(CONF_INCLUDE_BUILTIN_IDF_COMPONENTS, []):
        include_builtin_idf_component(component_name)

    # DHCP server: only disable if explicitly set to false
    # WiFi component handles its own optimization when AP mode is not used
    # When using Arduino with Ethernet, DHCP server functions must be available
    # for the Network library to compile, even if not actively used
    if advanced.get(CONF_ENABLE_LWIP_DHCP_SERVER) is False and not (
        conf[CONF_TYPE] == FRAMEWORK_ARDUINO and "ethernet" in CORE.loaded_integrations
    ):
        add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", False)
    if not advanced[CONF_ENABLE_LWIP_MDNS_QUERIES]:
        add_idf_sdkconfig_option("CONFIG_LWIP_DNS_SUPPORT_MDNS_QUERIES", False)
    if not advanced[CONF_ENABLE_LWIP_BRIDGE_INTERFACE]:
        add_idf_sdkconfig_option("CONFIG_LWIP_BRIDGEIF_MAX_PORTS", 0)

    _configure_lwip_max_sockets(conf)

    if advanced[CONF_EXECUTE_FROM_PSRAM]:
        if variant == VARIANT_ESP32S3:
            add_idf_sdkconfig_option("CONFIG_SPIRAM_FETCH_INSTRUCTIONS", True)
            add_idf_sdkconfig_option("CONFIG_SPIRAM_RODATA", True)
        elif variant == VARIANT_ESP32P4:
            add_idf_sdkconfig_option("CONFIG_SPIRAM_XIP_FROM_PSRAM", True)
        else:
            raise ValueError("Unhandled ESP32 variant")

    # Apply LWIP core locking for better socket performance
    # This is already enabled by default in Arduino framework, where it provides
    # significant performance benefits. Our benchmarks show socket operations are
    # 24-200% faster with core locking enabled:
    # - select() on 4 sockets: ~190μs (Arduino/core locking) vs ~235μs (ESP-IDF default)
    # - Up to 200% slower under load when all operations queue through tcpip_thread
    # Enabling this makes ESP-IDF socket performance match Arduino framework.
    if advanced[CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING]:
        add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_CORE_LOCKING", True)
    if advanced[CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY]:
        add_idf_sdkconfig_option("CONFIG_LWIP_CHECK_THREAD_SAFETY", True)

    # Disable placing libc locks in IRAM to save RAM
    # This is safe for ESPHome since no IRAM ISRs (interrupts that run while cache is disabled)
    # use libc lock APIs. Saves approximately 1.3KB (1,356 bytes) of IRAM.
    if advanced[CONF_DISABLE_LIBC_LOCKS_IN_IRAM]:
        add_idf_sdkconfig_option("CONFIG_LIBC_LOCKS_PLACE_IN_IRAM", False)

    # Disable VFS support for termios (terminal I/O functions)
    # USB Serial JTAG VFS functions require termios support.
    # Components that need it (e.g., logger when USB_SERIAL_JTAG is supported but not selected
    # as the logger output) call require_vfs_termios().
    # Saves approximately 1.8KB of flash when disabled (default).
    if CORE.data.get(KEY_VFS_TERMIOS_REQUIRED, False):
        # Component requires VFS termios - force enable regardless of user setting
        add_idf_sdkconfig_option("CONFIG_VFS_SUPPORT_TERMIOS", True)
    else:
        # No component needs it - allow user to control (default: disabled)
        add_idf_sdkconfig_option(
            "CONFIG_VFS_SUPPORT_TERMIOS", not advanced[CONF_DISABLE_VFS_SUPPORT_TERMIOS]
        )

    # Disable VFS support for select() with file descriptors
    # ESPHome only uses select() with sockets via lwip_select(), which still works.
    # VFS select is only needed for UART/eventfd file descriptors.
    # Components that need it (e.g., openthread) call require_vfs_select().
    # Saves approximately 2.7KB of flash when disabled (default).
    if CORE.data.get(KEY_VFS_SELECT_REQUIRED, False):
        # Component requires VFS select - force enable regardless of user setting
        add_idf_sdkconfig_option("CONFIG_VFS_SUPPORT_SELECT", True)
    else:
        # No component needs it - allow user to control (default: disabled)
        add_idf_sdkconfig_option(
            "CONFIG_VFS_SUPPORT_SELECT", not advanced[CONF_DISABLE_VFS_SUPPORT_SELECT]
        )

    # Disable VFS support for directory functions (opendir, readdir, mkdir, etc.)
    # ESPHome doesn't use directory functions on ESP32.
    # Components that need it (e.g., storage components) call require_vfs_dir().
    # Saves approximately 0.5KB+ of flash when disabled (default).
    if CORE.data.get(KEY_VFS_DIR_REQUIRED, False):
        # Component requires VFS directory support - force enable regardless of user setting
        add_idf_sdkconfig_option("CONFIG_VFS_SUPPORT_DIR", True)
    else:
        # No component needs it - allow user to control (default: disabled)
        add_idf_sdkconfig_option(
            "CONFIG_VFS_SUPPORT_DIR", not advanced[CONF_DISABLE_VFS_SUPPORT_DIR]
        )

    if use_platformio:
        cg.add_platformio_option("board_build.partitions", "partitions.csv")
    if CONF_PARTITIONS in config:
        if isinstance(config[CONF_PARTITIONS], list):
            for partition in config[CONF_PARTITIONS]:
                add_partition(
                    partition[CONF_NAME],
                    partition[CONF_TYPE],
                    partition[CONF_SUBTYPE],
                    partition[CONF_SIZE],
                )
        else:
            add_extra_build_file(
                "partitions.csv", CORE.relative_config_path(config[CONF_PARTITIONS])
            )

    if assertion_level := advanced.get(CONF_ASSERTION_LEVEL):
        for key, flag in ASSERTION_LEVELS.items():
            add_idf_sdkconfig_option(flag, assertion_level == key)

    add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_DEFAULT", False)
    compiler_optimization = advanced[CONF_COMPILER_OPTIMIZATION]
    for key, flag in COMPILER_OPTIMIZATIONS.items():
        add_idf_sdkconfig_option(flag, compiler_optimization == key)

    add_idf_sdkconfig_option(
        "CONFIG_LWIP_ESP_LWIP_ASSERT",
        conf[CONF_ADVANCED][CONF_ENABLE_LWIP_ASSERT],
    )

    if advanced[CONF_IGNORE_EFUSE_MAC_CRC]:
        add_idf_sdkconfig_option("CONFIG_ESP_MAC_IGNORE_MAC_CRC_ERROR", True)
        add_idf_sdkconfig_option("CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE", False)
    if advanced[CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES]:
        _LOGGER.warning(
            "Using experimental features in ESP-IDF may result in unexpected failures."
        )
        add_idf_sdkconfig_option("CONFIG_IDF_EXPERIMENTAL_FEATURES", True)
        if config[CONF_FLASH_SIZE] == "32MB":
            add_idf_sdkconfig_option(
                "CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH", True
            )

    # Enable OTA rollback support
    if advanced[CONF_ENABLE_OTA_ROLLBACK]:
        add_idf_sdkconfig_option("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE", True)
        cg.add_define("USE_OTA_ROLLBACK")

    # Enable signed app verification without hardware secure boot
    if signed_ota := advanced.get(CONF_SIGNED_OTA_VERIFICATION):
        add_idf_sdkconfig_option("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT", True)
        add_idf_sdkconfig_option("CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT", True)

        scheme = signed_ota[CONF_SIGNING_SCHEME]
        for key, flag in SIGNING_SCHEMES.items():
            add_idf_sdkconfig_option(flag, scheme == key)

        if CONF_SIGNING_KEY in signed_ota:
            # Private key mode — auto-sign binaries during build
            add_idf_sdkconfig_option("CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES", True)
            add_idf_sdkconfig_option(
                "CONFIG_SECURE_BOOT_SIGNING_KEY",
                str(signed_ota[CONF_SIGNING_KEY].resolve()),
            )
        else:
            # Public key mode — verification only, external signing required
            add_idf_sdkconfig_option("CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES", False)
            add_idf_sdkconfig_option(
                "CONFIG_SECURE_BOOT_VERIFICATION_KEY",
                str(signed_ota[CONF_VERIFICATION_KEY].resolve()),
            )

        cg.add_define("USE_OTA_SIGNED_VERIFICATION")

    cg.add_define("ESPHOME_LOOP_TASK_STACK_SIZE", advanced[CONF_LOOP_TASK_STACK_SIZE])

    cg.add_define(
        "USE_ESP_IDF_VERSION_CODE",
        cg.RawExpression(
            f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
        ),
    )

    add_idf_sdkconfig_option(f"CONFIG_LOG_DEFAULT_LEVEL_{conf[CONF_LOG_LEVEL]}", True)

    # Disable OpenOCD debug stubs to save code size
    # These are used for on-chip debugging with OpenOCD/JTAG, rarely needed for ESPHome
    if advanced[CONF_DISABLE_DEBUG_STUBS]:
        add_idf_sdkconfig_option("CONFIG_ESP_DEBUG_STUBS_ENABLE", False)

    # Disable OCD-aware exception handlers
    # When enabled, the panic handler detects JTAG debugger and halts instead of resetting
    # Most ESPHome users don't use JTAG debugging
    if advanced[CONF_DISABLE_OCD_AWARE]:
        add_idf_sdkconfig_option("CONFIG_ESP_DEBUG_OCDAWARE", False)

    # Disable USB Serial/JTAG secondary console
    # Components like logger can call require_usb_serial_jtag_secondary() to re-enable
    if CORE.data[KEY_ESP32].get(KEY_USB_SERIAL_JTAG_SECONDARY_REQUIRED, False):
        add_idf_sdkconfig_option("CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG", True)
    elif advanced[CONF_DISABLE_USB_SERIAL_JTAG_SECONDARY]:
        add_idf_sdkconfig_option("CONFIG_ESP_CONSOLE_SECONDARY_NONE", True)

    # Disable /dev/null VFS initialization
    # ESPHome doesn't typically need /dev/null
    if advanced[CONF_DISABLE_DEV_NULL_VFS]:
        add_idf_sdkconfig_option("CONFIG_VFS_INITIALIZE_DEV_NULL", False)

    # Disable keeping peer certificate after TLS handshake
    # Saves ~4KB heap per connection, but prevents certificate inspection after handshake
    # Components that need it can call require_mbedtls_peer_cert()
    if CORE.data[KEY_ESP32].get(KEY_MBEDTLS_PEER_CERT_REQUIRED, False):
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE", True)
    elif advanced[CONF_DISABLE_MBEDTLS_PEER_CERT]:
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE", False)

    # Disable PKCS#7 support in mbedTLS
    # Only needed for specific certificate validation scenarios
    # Components that need it can call require_mbedtls_pkcs7()
    if CORE.data[KEY_ESP32].get(KEY_MBEDTLS_PKCS7_REQUIRED, False):
        # Component called require_mbedtls_pkcs7() - enable regardless of user setting
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_PKCS7_C", True)
    elif advanced[CONF_DISABLE_MBEDTLS_PKCS7]:
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_PKCS7_C", False)

    # Disable SHA-384 and SHA-512 in mbedTLS
    # ESPHome doesn't use either algorithm. SHA-384 shares the same
    # compression function as SHA-512 (mbedtls_internal_sha512_process),
    # so both must be disabled to eliminate the ~3KB software fallback
    # that IDF 6.0's PSA parallel engine always links in.
    # On IDF < 6.0 these are a single config and hardware-only (no
    # software fallback), so there was no code size cost to leaving
    # them enabled.
    # Components that need SHA-384/SHA-512 can call require_mbedtls_sha512()
    if idf_version() >= cv.Version(6, 0, 0) and not CORE.data[KEY_ESP32].get(
        KEY_MBEDTLS_SHA512_REQUIRED, False
    ):
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_SHA384_C", False)
        add_idf_sdkconfig_option("CONFIG_MBEDTLS_SHA512_C", False)

    # Disable PicolibC Newlib compatibility shim on IDF 6.0+
    # IDF 6.0 switched from Newlib to PicolibC. The shim provides thread-local
    # stdin/stdout/stderr and getreent() for code compiled against Newlib.
    # ESPHome doesn't link against Newlib-built libraries that use stdio.
    # If a component needs it (e.g. precompiled Newlib binaries), re-enable via:
    #   esp32:
    #     framework:
    #       sdkconfig_options:
    #         CONFIG_LIBC_PICOLIBC_NEWLIB_COMPATIBILITY: "y"
    if idf_version() >= cv.Version(6, 0, 0):
        add_idf_sdkconfig_option("CONFIG_LIBC_PICOLIBC_NEWLIB_COMPATIBILITY", False)

    # Disable regi2c control functions in IRAM
    # Only needed if using analog peripherals (ADC, DAC, etc.) from ISRs while cache is disabled
    if advanced[CONF_DISABLE_REGI2C_IN_IRAM]:
        add_idf_sdkconfig_option("CONFIG_ESP_REGI2C_CTRL_FUNC_IN_IRAM", False)

    # Place ADC oneshot control functions in IRAM for cache safety
    # When flash cache is disabled (during NVS writes by WiFi, BLE, Zigbee, Thread,
    # power management, etc.), ADC reads will crash if these functions are in flash.
    # Components using ADC call require_adc_oneshot_iram() to force this.
    if (
        CORE.data[KEY_ESP32].get(KEY_ADC_ONESHOT_IRAM_REQUIRED, False)
        or advanced[CONF_ADC_ONESHOT_IN_IRAM]
    ):
        add_idf_sdkconfig_option("CONFIG_ADC_ONESHOT_CTRL_FUNC_IN_IRAM", True)

    # Disable FATFS support
    # Components that need FATFS (SD card, etc.) can call require_fatfs()
    if CORE.data[KEY_ESP32].get(KEY_FATFS_REQUIRED, False):
        # Component called require_fatfs() - enable regardless of user setting
        add_idf_sdkconfig_option("CONFIG_FATFS_LFN_NONE", False)
        add_idf_sdkconfig_option("CONFIG_FATFS_VOLUME_COUNT", 2)
    elif advanced[CONF_DISABLE_FATFS]:
        add_idf_sdkconfig_option("CONFIG_FATFS_LFN_NONE", True)
        add_idf_sdkconfig_option("CONFIG_FATFS_VOLUME_COUNT", 0)

    for name, value in conf[CONF_SDKCONFIG_OPTIONS].items():
        add_idf_sdkconfig_option(name, RawSdkconfigValue(value))

    # Components from YAML are added in a separate coroutine with FINAL priority
    # Schedule it to run after all other components
    if conf[CONF_COMPONENTS]:
        CORE.add_job(_add_yaml_idf_components, conf[CONF_COMPONENTS])

    # Write EXCLUDE_COMPONENTS at FINAL priority after all components have had
    # a chance to call include_builtin_idf_component() to re-enable components they need.
    # Default exclusions are added in set_core_data() during config validation.
    CORE.add_job(_write_exclude_components)

    # Write Arduino selective compilation sdkconfig at FINAL priority after all
    # components have had a chance to call cg.add_library() to enable libraries they need.
    if conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.add_job(_write_arduino_libraries_sdkconfig)


KEY_CUSTOM_PARTITIONS = "custom_partitions"


@dataclass
class PartitionEntry:
    name: str
    type: str
    subtype: str
    size: int


# Partition sizes (offsets auto-placed by gen_esp32part.py).
# These constants are the single source of truth — used in both
# the CSV generation and the overhead calculation.
BOOTLOADER_SIZE = 0x8000
PARTITION_TABLE_SIZE = 0x1000
FIRST_PARTITION_OFFSET = BOOTLOADER_SIZE + PARTITION_TABLE_SIZE
OTADATA_SIZE = 0x2000
PHY_INIT_SIZE = 0x1000
EEPROM_SIZE = 0x1000  # Arduino only
SPIFFS_SIZE = 0xF000  # Arduino only
ARDUINO_NVS_SIZE = 0x60000
IDF_NVS_SIZE = 0x70000


def _get_partition_overhead() -> int:
    """Total non-app partition budget (system partitions + nvs + padding).

    Custom partitions are appended at the end and steal from app.
    """
    # otadata + phy_init are followed by app0 which requires 64KB alignment,
    # so pad up to the next 64KB boundary.
    overhead = (
        FIRST_PARTITION_OFFSET + OTADATA_SIZE + PHY_INIT_SIZE + 0xFFFF
    ) & ~0xFFFF
    if CORE.using_arduino:
        overhead += EEPROM_SIZE + SPIFFS_SIZE + ARDUINO_NVS_SIZE
    else:
        overhead += IDF_NVS_SIZE
    return overhead


VALID_SUBTYPES: dict[str, set[str]] = {
    "app": VALID_APP_SUBTYPES,
    "data": VALID_DATA_SUBTYPES,
}


def _validate_partition(
    name: str, p_type: str | int, subtype: str | int, size: int
) -> None:
    """Validate partition parameters. Raises ValueError on invalid input."""
    if name in RESERVED_PARTITION_NAMES:
        raise ValueError(f"Partition name '{name}' is reserved.")
    if size % 0x1000 != 0:
        raise ValueError("Partition size must be 4KB (0x1000) aligned.")
    # Numeric or already-normalized hex types/subtypes skip string validation
    if not isinstance(p_type, str) or p_type.startswith("0x"):
        return
    if p_type not in VALID_SUBTYPES:
        raise ValueError(
            f"Type '{p_type}' is invalid. Only 'app' and 'data' are allowed."
            " Use numbers for custom types."
        )
    if not isinstance(subtype, str) or subtype.startswith("0x"):
        return
    valid = VALID_SUBTYPES[p_type]
    if subtype not in valid:
        raise ValueError(
            f"Subtype '{subtype}' is invalid for {p_type} type."
            f" Only {', '.join(sorted(valid))} are allowed."
            " Use numbers for custom subtypes."
        )


def add_partition(name: str, p_type: str | int, subtype: str | int, size: int) -> None:
    """Register a custom partition to be appended to the partition table.

    Called from component to_code() to request additional flash partitions.
    Size must be 4KB aligned. Integer types/subtypes are converted to hex strings.
    """
    if name in CORE.data[KEY_ESP32].get(KEY_CUSTOM_PARTITIONS, {}):
        raise ValueError(f"Partition name '{name}' is already defined.")
    _validate_partition(name, p_type, subtype, size)
    p_type_str = f"0x{p_type:X}" if isinstance(p_type, int) else p_type
    subtype_str = f"0x{subtype:X}" if isinstance(subtype, int) else subtype
    custom_partitions = CORE.data[KEY_ESP32].setdefault(KEY_CUSTOM_PARTITIONS, {})
    custom_partitions[name] = PartitionEntry(
        name=name, type=p_type_str, subtype=subtype_str, size=size
    )


def _flash_size_to_bytes(flash_size_mb: str) -> int:
    """Convert flash size string (e.g. '4MB') to bytes."""
    return int(flash_size_mb.removesuffix("MB")) * 1024 * 1024


def _get_custom_partitions_total_size() -> int:
    """Total size of custom partitions including alignment padding."""
    size = 0
    for partition in CORE.data[KEY_ESP32].get(KEY_CUSTOM_PARTITIONS, {}).values():
        if partition.type == "app":
            size = (size + 0xFFFF) & ~0xFFFF  # align to 64KB
        size += partition.size
    return size


def _get_app_partition_size(flash_size_mb: str) -> int:
    flash_bytes = _flash_size_to_bytes(flash_size_mb)
    custom_total = _get_custom_partitions_total_size()
    # Align down to 64KB — app partitions require 64KB-aligned offsets,
    # so the size must also be aligned to avoid unbudgeted padding.
    raw_size = (flash_bytes - _get_partition_overhead() - custom_total) // 2
    app_size = raw_size & ~0xFFFF
    wasted = (raw_size - app_size) * 2
    if wasted:
        _LOGGER.info(
            "Custom partitions cause %dKB of wasted flash due to 64KB app partition alignment.",
            wasted // 1024,
        )
    if app_size <= 0x10000:  # 64 KB
        raise ValueError(
            "Custom partitions are too large to fit in the available flash size. "
            "Reduce custom partition sizes."
        )
    if app_size <= 0x80000:  # 512 KB
        _LOGGER.warning(
            "App partition size is only %dKB. This may be too small for firmware with "
            "many components. Consider reducing custom partition sizes or using a "
            "larger flash chip.",
            app_size // 1024,
        )
    return app_size


def get_partition_csv(flash_size_mb: str) -> str:
    app_size = _get_app_partition_size(flash_size_mb)

    partitions: list[PartitionEntry] = [
        PartitionEntry(name="otadata", type="data", subtype="ota", size=OTADATA_SIZE),
        PartitionEntry(name="phy_init", type="data", subtype="phy", size=PHY_INIT_SIZE),
        PartitionEntry(name="app0", type="app", subtype="ota_0", size=app_size),
        PartitionEntry(name="app1", type="app", subtype="ota_1", size=app_size),
    ]
    if CORE.using_arduino:
        partitions.append(
            PartitionEntry(name="eeprom", type="data", subtype="0x99", size=EEPROM_SIZE)
        )
        partitions.append(
            PartitionEntry(
                name="spiffs", type="data", subtype="spiffs", size=SPIFFS_SIZE
            )
        )
        partitions.append(
            PartitionEntry(
                name="nvs", type="data", subtype="nvs", size=ARDUINO_NVS_SIZE
            )
        )
    else:
        partitions.append(
            PartitionEntry(name="nvs", type="data", subtype="nvs", size=IDF_NVS_SIZE)
        )
    partitions.extend(CORE.data[KEY_ESP32].get(KEY_CUSTOM_PARTITIONS, {}).values())

    csv = "".join(
        f"{p.name}, {p.type}, {p.subtype}, , 0x{p.size:X},\n" for p in partitions
    )
    _LOGGER.debug("Partition table:\n%s", csv)
    return csv


def _format_sdkconfig_val(value: SdkconfigValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, RawSdkconfigValue):
        return value.value
    raise ValueError


def _write_sdkconfig():
    # sdkconfig.{name} stores the real sdkconfig (modified by esp-idf with default)
    # sdkconfig.{name}.esphomeinternal stores what esphome last wrote
    # we use the internal one to detect if there were any changes, and if so write them to the
    # real sdkconfig
    sdk_path = Path(CORE.relative_build_path(f"sdkconfig.{CORE.name}"))
    internal_path = Path(
        CORE.relative_build_path(f"sdkconfig.{CORE.name}.esphomeinternal")
    )

    want_opts = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    contents = (
        "\n".join(
            f"{name}={_format_sdkconfig_val(value)}"
            for name, value in sorted(want_opts.items())
        )
        + "\n"
    )

    if write_file_if_changed(internal_path, contents):
        # internal changed, update real one
        write_file_if_changed(sdk_path, contents)
        clean_build(clear_pio_cache=False)


def _platformio_library_to_dependency(library: Library) -> tuple[str, dict[str, str]]:
    dependency: dict[str, str] = {}
    name, version, path = generate_idf_component(library)
    dependency["override_path"] = str(path)
    dependency["version"] = version
    return name, dependency


def _write_idf_component_yml():
    yml_path = CORE.relative_build_path("src/idf_component.yml")
    dependencies: dict[str, dict] = {}

    # For Arduino builds, override unused managed components from the Arduino framework
    # by pointing them to empty stub directories using override_path
    # This prevents the IDF component manager from downloading the real components
    if CORE.using_arduino:
        # Determine which IDF components are needed by enabled Arduino libraries
        enabled_libs = CORE.data[KEY_ESP32].get(KEY_ARDUINO_LIBRARIES, set())
        required_idf_components = {
            comp
            for lib in enabled_libs
            for comp in ARDUINO_LIBRARY_IDF_COMPONENTS.get(lib, ())
        }

        # Only stub components that are not required by any enabled Arduino library
        components_to_stub = (
            set(ARDUINO_EXCLUDED_IDF_COMPONENTS) - required_idf_components
        )

        stubs_dir = CORE.relative_build_path("component_stubs")
        stubs_dir.mkdir(exist_ok=True)
        for component_name in components_to_stub:
            # Create stub directory with minimal CMakeLists.txt
            stub_path = stubs_dir / _idf_component_stub_name(component_name)
            stub_path.mkdir(exist_ok=True)
            stub_cmake = stub_path / "CMakeLists.txt"
            if not stub_cmake.exists():
                stub_cmake.write_text("idf_component_register()\n")
            dependencies[_idf_component_dep_name(component_name)] = {
                "version": "*",
                "override_path": str(stub_path),
            }

        # Remove stubs for components that are now required by enabled libraries
        for component_name in required_idf_components:
            stub_path = stubs_dir / _idf_component_stub_name(component_name)
            if stub_path.exists():
                rmtree(stub_path)

        if CORE.using_toolchain_esp_idf:
            add_idf_component(
                name="espressif/arduino-esp32",
                ref=str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
            )

    if CORE.using_toolchain_esp_idf:
        # Try to convert PlatformIO library to ESP-IDF components
        for name, library in CORE.platformio_libraries.items():
            # Don't process arduino libraries
            if name in ARDUINO_DISABLED_LIBRARIES:
                continue
            dependency_name, dependency = _platformio_library_to_dependency(library)
            dependencies[dependency_name] = dependency

    if CORE.data[KEY_ESP32][KEY_COMPONENTS]:
        components: dict = CORE.data[KEY_ESP32][KEY_COMPONENTS]
        for name, component in components.items():
            dependency: dict[str, str] = {}
            if component[KEY_REF]:
                dependency["version"] = component[KEY_REF]
            if component[KEY_REPO]:
                dependency["git"] = component[KEY_REPO]
            if component[KEY_PATH]:
                dependency["path"] = component[KEY_PATH]
            dependencies[name] = dependency

    contents = yaml_util.dump({"dependencies": dependencies}) if dependencies else ""
    if write_file_if_changed(yml_path, contents):
        dependencies_lock = CORE.relative_build_path("dependencies.lock")
        if dependencies_lock.is_file():
            dependencies_lock.unlink()
        clean_cmake_cache()


# Called by writer.py
def copy_files():
    _write_sdkconfig()
    _write_idf_component_yml()

    if "partitions.csv" not in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES]:
        flash_size = CORE.data[KEY_ESP32][KEY_FLASH_SIZE]
        write_file_if_changed(
            CORE.relative_build_path("partitions.csv"),
            get_partition_csv(flash_size),
        )
    # IDF build scripts look for version string to put in the build.
    # However, if the build path does not have an initialized git repo,
    # and no version.txt file exists, the CMake script fails for some setups.
    # Fix by manually pasting a version.txt file, containing the ESPHome version
    write_file_if_changed(
        CORE.relative_build_path("version.txt"),
        __version__,
    )

    for file in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES].values():
        name: str = file[KEY_NAME]
        path: Path = file[KEY_PATH]
        if str(path).startswith("http"):
            import requests

            CORE.relative_build_path(name).parent.mkdir(parents=True, exist_ok=True)
            content = requests.get(path, timeout=30).content
            CORE.relative_build_path(name).write_bytes(content)
        else:
            copy_file_if_changed(path, CORE.relative_build_path(name))


def _decode_pc(config, addr):
    from esphome.platformio import toolchain

    idedata = toolchain.get_idedata(config)
    if not idedata.addr2line_path or not idedata.firmware_elf_path:
        _LOGGER.debug("decode_pc no addr2line")
        return
    command = [idedata.addr2line_path, "-pfiaC", "-e", idedata.firmware_elf_path, addr]
    try:
        translation = subprocess.check_output(command, close_fds=False).decode().strip()
    except Exception:  # pylint: disable=broad-except
        _LOGGER.debug("Caught exception for command %s", command, exc_info=1)
        return

    if "?? ??:0" in translation:
        # Nothing useful
        return
    translation = translation.replace(" at ??:?", "").replace(":?", "")
    _LOGGER.warning("Decoded %s", translation)


def _parse_register(config, regex, line):
    match = regex.match(line)
    if match is not None:
        _decode_pc(config, match.group(1))


STACKTRACE_ESP32_PC_RE = re.compile(r".*PC\s*:\s*(?:0x)?(4[0-9a-fA-F]{7}).*")
STACKTRACE_ESP32_EXCVADDR_RE = re.compile(r"EXCVADDR\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_ESP32_C3_PC_RE = re.compile(r"MEPC\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_ESP32_C3_RA_RE = re.compile(r"RA\s*:\s*(?:0x)?(4[0-9a-fA-F]{7})")
STACKTRACE_BAD_ALLOC_RE = re.compile(
    r"^last failed alloc call: (4[0-9a-fA-F]{7})\((\d+)\)$"
)
STACKTRACE_ESP32_BACKTRACE_RE = re.compile(
    r"Backtrace:(?:\s*0x[0-9a-fA-F]{8}:0x[0-9a-fA-F]{8})+"
)
STACKTRACE_ESP32_BACKTRACE_PC_RE = re.compile(r"4[0-9a-f]{7}")
# ESP32 crash handler (stored backtrace from previous boot)
STACKTRACE_ESP32_CRASH_BT_RE = re.compile(r"BT\d+:\s*0x([0-9a-fA-F]{8})")


def process_stacktrace(config, line, backtrace_state):
    line = line.strip()

    # ESP32 PC/EXCVADDR
    _parse_register(config, STACKTRACE_ESP32_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP32_EXCVADDR_RE, line)
    # ESP32-C3 PC/RA
    _parse_register(config, STACKTRACE_ESP32_C3_PC_RE, line)
    _parse_register(config, STACKTRACE_ESP32_C3_RA_RE, line)

    # bad alloc
    match = re.match(STACKTRACE_BAD_ALLOC_RE, line)
    if match is not None:
        _LOGGER.warning(
            "Memory allocation of %s bytes failed at %s", match.group(2), match.group(1)
        )
        _decode_pc(config, match.group(1))

    # ESP32 crash handler backtrace (from previous boot)
    match = re.search(STACKTRACE_ESP32_CRASH_BT_RE, line)
    if match is not None:
        _decode_pc(config, match.group(1))

    # ESP32 single-line backtrace
    match = re.match(STACKTRACE_ESP32_BACKTRACE_RE, line)
    if match is not None:
        _LOGGER.warning("Found stack trace! Trying to decode it")
        for addr in re.finditer(STACKTRACE_ESP32_BACKTRACE_PC_RE, line):
            _decode_pc(config, addr.group())

    return backtrace_state
