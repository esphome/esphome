from dataclasses import dataclass
import itertools
import logging
import os
from pathlib import Path

from esphome import yaml_util
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_COMPONENTS,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_IGNORE_EFUSE_CUSTOM_MAC,
    CONF_IGNORE_EFUSE_MAC_CRC,
    CONF_NAME,
    CONF_PATH,
    CONF_PLATFORM_VERSION,
    CONF_PLATFORMIO_OPTIONS,
    CONF_REF,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_NAME,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    ThreadModel,
    __version__,
)
from esphome.core import CORE, HexInt, TimePeriod
from esphome.cpp_generator import RawExpression
import esphome.final_validate as fv
from esphome.helpers import copy_file_if_changed, mkdir_p, write_file_if_changed
from esphome.types import ConfigType

from .boards import BOARDS, STANDARD_BOARDS
from .const import (  # noqa
    KEY_BOARD,
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_EXTRA_BUILD_FILES,
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
CONF_ENABLE_LWIP_ASSERT = "enable_lwip_assert"
CONF_EXECUTE_FROM_PSRAM = "execute_from_psram"
CONF_RELEASE = "release"

ASSERTION_LEVELS = {
    "DISABLE": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE",
    "ENABLE": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE",
    "SILENT": "CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT",
}

COMPILER_OPTIMIZATIONS = {
    "DEBUG": "CONFIG_COMPILER_OPTIMIZATION_DEBUG",
    "NONE": "CONFIG_COMPILER_OPTIMIZATION_NONE",
    "PERF": "CONFIG_COMPILER_OPTIMIZATION_PERF",
    "SIZE": "CONFIG_COMPILER_OPTIMIZATION_SIZE",
}

ARDUINO_ALLOWED_VARIANTS = [
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
]


def get_cpu_frequencies(*frequencies):
    return [str(x) + "MHZ" for x in frequencies]


CPU_FREQUENCIES = {
    VARIANT_ESP32: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32S2: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32S3: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32C2: get_cpu_frequencies(80, 120),
    VARIANT_ESP32C3: get_cpu_frequencies(80, 160),
    VARIANT_ESP32C5: get_cpu_frequencies(80, 160, 240),
    VARIANT_ESP32C6: get_cpu_frequencies(80, 120, 160),
    VARIANT_ESP32H2: get_cpu_frequencies(16, 32, 48, 64, 96),
    VARIANT_ESP32P4: get_cpu_frequencies(40, 360, 400),
}

# Make sure not missed here if a new variant added.
assert all(v in CPU_FREQUENCIES for v in VARIANTS)

FULL_CPU_FREQUENCIES = set(itertools.chain.from_iterable(CPU_FREQUENCIES.values()))


def set_core_data(config):
    cpu_frequency = config.get(CONF_CPU_FREQUENCY, None)
    variant = config[CONF_VARIANT]
    # if not specified in config, set to 160MHz if supported, the fastest otherwise
    if cpu_frequency is None:
        choices = CPU_FREQUENCIES[variant]
        if "160MHZ" in choices:
            cpu_frequency = "160MHZ"
        elif "360MHZ" in choices:
            cpu_frequency = "360MHZ"
        else:
            cpu_frequency = choices[-1]
        config[CONF_CPU_FREQUENCY] = cpu_frequency
    elif cpu_frequency not in CPU_FREQUENCIES[variant]:
        raise cv.Invalid(
            f"Invalid CPU frequency '{cpu_frequency}' for {config[CONF_VARIANT]}",
            path=[CONF_CPU_FREQUENCY],
        )

    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_ESP32
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS] = {}
        CORE.data[KEY_ESP32][KEY_COMPONENTS] = {}
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
        if variant not in ARDUINO_ALLOWED_VARIANTS:
            raise cv.Invalid(
                f"ESPHome does not support using the Arduino framework for the {variant}. Please use the ESP-IDF framework instead.",
                path=[CONF_FRAMEWORK, CONF_TYPE],
            )
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )

    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ESP32][KEY_VARIANT] = variant
    CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES] = {}

    return config


def get_esp32_variant(core_obj=None):
    return (core_obj or CORE).data[KEY_ESP32][KEY_VARIANT]


def get_board(core_obj=None):
    return (core_obj or CORE).data[KEY_ESP32][KEY_BOARD]


def get_download_types(storage_json):
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
    if not CORE.using_esp_idf:
        raise ValueError("Not an esp-idf project")
    CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][name] = value


def add_idf_component(
    *,
    name: str,
    repo: str = None,
    ref: str = None,
    path: str = None,
    refresh: TimePeriod = None,
    components: list[str] | None = None,
    submodules: list[str] | None = None,
):
    """Add an esp-idf component to the project."""
    if not CORE.using_esp_idf:
        raise ValueError("Not an esp-idf project")
    if not repo and not ref and not path:
        raise ValueError("Requires at least one of repo, ref or path")
    if refresh or submodules or components:
        _LOGGER.warning(
            "The refresh, components and submodules parameters in add_idf_component() are "
            "deprecated and will be removed in ESPHome 2026.1. If you are seeing this, report "
            "an issue to the external_component author and ask them to update it."
        )
    if components:
        for comp in components:
            CORE.data[KEY_ESP32][KEY_COMPONENTS][comp] = {
                KEY_REPO: repo,
                KEY_REF: ref,
                KEY_PATH: f"{path}/{comp}" if path else comp,
            }
    else:
        CORE.data[KEY_ESP32][KEY_COMPONENTS][name] = {
            KEY_REPO: repo,
            KEY_REF: ref,
            KEY_PATH: path,
        }


def add_extra_script(stage: str, filename: str, path: str):
    """Add an extra script to the project."""
    key = f"{stage}:{filename}"
    if add_extra_build_file(filename, path):
        cg.add_platformio_option("extra_scripts", [key])


def add_extra_build_file(filename: str, path: str) -> bool:
    """Add an extra build file to the project."""
    if filename not in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES]:
        CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES][filename] = {
            KEY_NAME: filename,
            KEY_PATH: path,
        }
        return True
    return False


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/espressif/arduino-esp32/releases) version to
    # a PIO pioarduino/framework-arduinoespressif32 value
    return f"pioarduino/framework-arduinoespressif32@https://github.com/espressif/arduino-esp32/releases/download/{str(ver)}/esp32-{str(ver)}.zip"


def _format_framework_espidf_version(
    ver: cv.Version, release: str, for_platformio: bool
) -> str:
    # format the given arduino (https://github.com/espressif/esp-idf/releases) version to
    # a PIO platformio/framework-espidf value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
    if for_platformio:
        return f"platformio/framework-espidf@~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    if release:
        return f"pioarduino/framework-espidf@https://github.com/pioarduino/esp-idf/releases/download/v{str(ver)}.{release}/esp-idf-v{str(ver)}.zip"
    return f"pioarduino/framework-espidf@https://github.com/pioarduino/esp-idf/releases/download/v{str(ver)}/esp-idf-v{str(ver)}.zip"


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)

# The default/recommended arduino framework version
#  - https://github.com/espressif/arduino-esp32/releases
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(3, 2, 1)
# The platform-espressif32 version to use for arduino frameworks
#  - https://github.com/pioarduino/platform-espressif32/releases
ARDUINO_PLATFORM_VERSION = cv.Version(54, 3, 21, "2")

# The default/recommended esp-idf framework version
#  - https://github.com/espressif/esp-idf/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION = cv.Version(5, 4, 2)
# The platformio/espressif32 version to use for esp-idf frameworks
#  - https://github.com/platformio/platform-espressif32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif32
ESP_IDF_PLATFORM_VERSION = cv.Version(54, 3, 21, "2")

# List based on https://registry.platformio.org/tools/platformio/framework-espidf/versions
SUPPORTED_PLATFORMIO_ESP_IDF_5X = [
    cv.Version(5, 3, 1),
    cv.Version(5, 3, 0),
    cv.Version(5, 2, 2),
    cv.Version(5, 2, 1),
    cv.Version(5, 1, 2),
    cv.Version(5, 1, 1),
    cv.Version(5, 1, 0),
    cv.Version(5, 0, 2),
    cv.Version(5, 0, 1),
    cv.Version(5, 0, 0),
]

# pioarduino versions that don't require a release number
# List based on https://github.com/pioarduino/esp-idf/releases
SUPPORTED_PIOARDUINO_ESP_IDF_5X = [
    cv.Version(5, 5, 0),
    cv.Version(5, 4, 2),
    cv.Version(5, 4, 1),
    cv.Version(5, 4, 0),
    cv.Version(5, 3, 3),
    cv.Version(5, 3, 2),
    cv.Version(5, 3, 1),
    cv.Version(5, 3, 0),
    cv.Version(5, 1, 5),
    cv.Version(5, 1, 6),
]


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(3, 2, 1), "https://github.com/espressif/arduino-esp32.git"),
        "latest": (cv.Version(3, 2, 1), None),
        "recommended": (RECOMMENDED_ARDUINO_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_arduino_version(version)

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ARDUINO_PLATFORM_VERSION))
    )

    if value[CONF_SOURCE].startswith("http"):
        # prefix is necessary or platformio will complain with a cryptic error
        value[CONF_SOURCE] = f"framework-arduinoespressif32@{value[CONF_SOURCE]}"

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return value


def _esp_idf_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(5, 4, 2), "https://github.com/espressif/esp-idf.git"),
        "latest": (cv.Version(5, 2, 2), None),
        "recommended": (RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    if version < cv.Version(5, 0, 0):
        raise cv.Invalid("Only ESP-IDF 5.0+ is supported.")

    # flag this for later *before* we set value[CONF_PLATFORM_VERSION] below
    has_platform_ver = CONF_PLATFORM_VERSION in value

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ESP_IDF_PLATFORM_VERSION))
    )

    if (
        is_platformio := _platform_is_platformio(value[CONF_PLATFORM_VERSION])
    ) and version not in SUPPORTED_PLATFORMIO_ESP_IDF_5X:
        raise cv.Invalid(
            f"ESP-IDF {str(version)} not supported by platformio/espressif32"
        )

    if (
        version in SUPPORTED_PLATFORMIO_ESP_IDF_5X
        and version not in SUPPORTED_PIOARDUINO_ESP_IDF_5X
    ) and not has_platform_ver:
        raise cv.Invalid(
            f"ESP-IDF {value[CONF_VERSION]} may be supported by platformio/espressif32; please specify '{CONF_PLATFORM_VERSION}'"
        )

    if (
        not is_platformio
        and CONF_RELEASE not in value
        and version not in SUPPORTED_PIOARDUINO_ESP_IDF_5X
    ):
        raise cv.Invalid(
            f"ESP-IDF {value[CONF_VERSION]} is not available with pioarduino; you may need to specify '{CONF_RELEASE}'"
        )

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_espidf_version(
        version, value.get(CONF_RELEASE, None), is_platformio
    )

    if value[CONF_SOURCE].startswith("http"):
        # prefix is necessary or platformio will complain with a cryptic error
        value[CONF_SOURCE] = f"framework-espidf@{value[CONF_SOURCE]}"

    if version != RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected ESP-IDF framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return value


def _parse_platform_version(value):
    try:
        ver = cv.Version.parse(cv.version_number(value))
        if ver.major >= 50:  # a pioarduino version
            release = f"{ver.major}.{ver.minor:02d}.{ver.patch:02d}"
            if ver.extra:
                release += f"-{ver.extra}"
            return f"https://github.com/pioarduino/platform-espressif32/releases/download/{release}/platform-espressif32.zip"
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/espressif32@{value}"
    except cv.Invalid:
        return value


def _platform_is_platformio(value):
    try:
        ver = cv.Version.parse(cv.version_number(value))
        return ver.major < 50
    except cv.Invalid:
        return "platformio" in value


def _detect_variant(value):
    board = value.get(CONF_BOARD)
    variant = value.get(CONF_VARIANT)
    if variant and board is None:
        # If variant is set, we can derive the board from it
        # variant has already been validated against the known set
        value = value.copy()
        value[CONF_BOARD] = STANDARD_BOARDS[variant]
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
    if (
        config[CONF_VARIANT] != VARIANT_ESP32
        and CONF_ADVANCED in (conf_fw := config[CONF_FRAMEWORK])
        and CONF_IGNORE_EFUSE_MAC_CRC in conf_fw[CONF_ADVANCED]
    ):
        errs.append(
            cv.Invalid(
                f"'{CONF_IGNORE_EFUSE_MAC_CRC}' is not supported on {config[CONF_VARIANT]}",
                path=[CONF_FRAMEWORK, CONF_ADVANCED, CONF_IGNORE_EFUSE_MAC_CRC],
            )
        )
    if (
        config.get(CONF_FRAMEWORK, {})
        .get(CONF_ADVANCED, {})
        .get(CONF_EXECUTE_FROM_PSRAM)
    ):
        if config[CONF_VARIANT] != VARIANT_ESP32S3:
            errs.append(
                cv.Invalid(
                    f"'{CONF_EXECUTE_FROM_PSRAM}' is only supported on {VARIANT_ESP32S3} variant",
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

    if errs:
        raise cv.MultipleInvalid(errs)

    return config


ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
            cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
                {
                    cv.Optional(
                        CONF_IGNORE_EFUSE_CUSTOM_MAC, default=False
                    ): cv.boolean,
                }
            ),
        }
    ),
    _arduino_check_versions,
)

CONF_SDKCONFIG_OPTIONS = "sdkconfig_options"
CONF_ENABLE_LWIP_DHCP_SERVER = "enable_lwip_dhcp_server"
CONF_ENABLE_LWIP_MDNS_QUERIES = "enable_lwip_mdns_queries"
CONF_ENABLE_LWIP_BRIDGE_INTERFACE = "enable_lwip_bridge_interface"
CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING = "enable_lwip_tcpip_core_locking"
CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY = "enable_lwip_check_thread_safety"


def _validate_idf_component(config: ConfigType) -> ConfigType:
    """Validate IDF component config and warn about deprecated options."""
    if CONF_REFRESH in config:
        _LOGGER.warning(
            "The 'refresh' option for IDF components is deprecated and has no effect. "
            "It will be removed in ESPHome 2026.1. Please remove it from your configuration."
        )
    return config


ESP_IDF_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_RELEASE): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
            cv.Optional(CONF_SDKCONFIG_OPTIONS, default={}): {
                cv.string_strict: cv.string_strict
            },
            cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
                {
                    cv.Optional(CONF_ASSERTION_LEVEL): cv.one_of(
                        *ASSERTION_LEVELS, upper=True
                    ),
                    cv.Optional(CONF_COMPILER_OPTIMIZATION, default="SIZE"): cv.one_of(
                        *COMPILER_OPTIMIZATIONS, upper=True
                    ),
                    cv.Optional(CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES): cv.boolean,
                    cv.Optional(CONF_ENABLE_LWIP_ASSERT, default=True): cv.boolean,
                    cv.Optional(
                        CONF_IGNORE_EFUSE_CUSTOM_MAC, default=False
                    ): cv.boolean,
                    cv.Optional(CONF_IGNORE_EFUSE_MAC_CRC): cv.boolean,
                    # DHCP server is needed for WiFi AP mode. When WiFi component is used,
                    # it will handle disabling DHCP server when AP is not configured.
                    # Default to false (disabled) when WiFi is not used.
                    cv.OnlyWithout(
                        CONF_ENABLE_LWIP_DHCP_SERVER, "wifi", default=False
                    ): cv.boolean,
                    cv.Optional(
                        CONF_ENABLE_LWIP_MDNS_QUERIES, default=True
                    ): cv.boolean,
                    cv.Optional(
                        CONF_ENABLE_LWIP_BRIDGE_INTERFACE, default=False
                    ): cv.boolean,
                    cv.Optional(
                        CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING, default=True
                    ): cv.boolean,
                    cv.Optional(
                        CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY, default=True
                    ): cv.boolean,
                    cv.Optional(CONF_EXECUTE_FROM_PSRAM): cv.boolean,
                }
            ),
            cv.Optional(CONF_COMPONENTS, default=[]): cv.ensure_list(
                cv.All(
                    cv.Schema(
                        {
                            cv.Required(CONF_NAME): cv.string_strict,
                            cv.Optional(CONF_SOURCE): cv.git_ref,
                            cv.Optional(CONF_REF): cv.string,
                            cv.Optional(CONF_PATH): cv.string,
                            cv.Optional(CONF_REFRESH): cv.All(
                                cv.string, cv.source_refresh
                            ),
                        }
                    ),
                    _validate_idf_component,
                )
            ),
        }
    ),
    _esp_idf_check_versions,
)


class _FrameworkMigrationWarning:
    shown = False


def _show_framework_migration_message(name: str, variant: str) -> None:
    """Show a friendly message about framework migration when defaulting to Arduino."""
    if _FrameworkMigrationWarning.shown:
        return
    _FrameworkMigrationWarning.shown = True

    from esphome.log import AnsiFore, color

    message = (
        color(
            AnsiFore.BOLD_CYAN,
            f"💡 IMPORTANT: {name} doesn't have a framework specified!",
        )
        + "\n\n"
        + f"Currently, {variant} defaults to the Arduino framework.\n"
        + color(AnsiFore.YELLOW, "This will change to ESP-IDF in ESPHome 2026.1.0.\n")
        + "\n"
        + "Note: Newer ESP32 variants (C6, H2, P4, etc.) already use ESP-IDF by default.\n"
        + "\n"
        + "Why change? ESP-IDF offers:\n"
        + color(AnsiFore.GREEN, "  ✨ Up to 40% smaller binaries\n")
        + color(AnsiFore.GREEN, "  🚀 Better performance and optimization\n")
        + color(AnsiFore.GREEN, "  📦 Custom-built firmware for your exact needs\n")
        + color(
            AnsiFore.GREEN,
            "  🔧 Active development and testing by ESPHome developers\n",
        )
        + "\n"
        + "Trade-offs:\n"
        + color(AnsiFore.YELLOW, "  ⏱️  Compile times are ~25% longer\n")
        + color(AnsiFore.YELLOW, "  🔄 Some components need migration\n")
        + "\n"
        + "What should I do?\n"
        + color(AnsiFore.CYAN, "  Option 1")
        + ": Migrate to ESP-IDF (recommended)\n"
        + "    Add this to your YAML under 'esp32:':\n"
        + color(AnsiFore.WHITE, "      framework:\n")
        + color(AnsiFore.WHITE, "        type: esp-idf\n")
        + "\n"
        + color(AnsiFore.CYAN, "  Option 2")
        + ": Keep using Arduino (still supported)\n"
        + "    Add this to your YAML under 'esp32:':\n"
        + color(AnsiFore.WHITE, "      framework:\n")
        + color(AnsiFore.WHITE, "        type: arduino\n")
        + "\n"
        + "Need help? Check out the migration guide:\n"
        + color(
            AnsiFore.BLUE,
            "https://esphome.io/guides/esp32_arduino_to_idf.html",
        )
    )
    _LOGGER.warning(message)


def _set_default_framework(config):
    if CONF_FRAMEWORK not in config:
        config = config.copy()

        variant = config[CONF_VARIANT]
        if variant in ARDUINO_ALLOWED_VARIANTS:
            config[CONF_FRAMEWORK] = ARDUINO_FRAMEWORK_SCHEMA({})
            config[CONF_FRAMEWORK][CONF_TYPE] = FRAMEWORK_ARDUINO
            # Show the migration message
            _show_framework_migration_message(
                config.get(CONF_NAME, "This device"), variant
            )
        else:
            config[CONF_FRAMEWORK] = ESP_IDF_FRAMEWORK_SCHEMA({})
            config[CONF_FRAMEWORK][CONF_TYPE] = FRAMEWORK_ESP_IDF

    return config


FRAMEWORK_ESP_IDF = "esp-idf"
FRAMEWORK_ARDUINO = "arduino"
FRAMEWORK_SCHEMA = cv.typed_schema(
    {
        FRAMEWORK_ESP_IDF: ESP_IDF_FRAMEWORK_SCHEMA,
        FRAMEWORK_ARDUINO: ARDUINO_FRAMEWORK_SCHEMA,
    },
    lower=True,
    space="-",
)


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
            cv.Optional(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_CPU_FREQUENCY): cv.one_of(
                *FULL_CPU_FREQUENCIES, upper=True
            ),
            cv.Optional(CONF_FLASH_SIZE, default="4MB"): cv.one_of(
                *FLASH_SIZES, upper=True
            ),
            cv.Optional(CONF_PARTITIONS): cv.file_,
            cv.Optional(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
            cv.Optional(CONF_FRAMEWORK): FRAMEWORK_SCHEMA,
        }
    ),
    _detect_variant,
    _set_default_framework,
    set_core_data,
    cv.has_at_least_one_key(CONF_BOARD, CONF_VARIANT),
)


FINAL_VALIDATE_SCHEMA = cv.Schema(final_validate)


async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_platformio_option("board_upload.flash_size", config[CONF_FLASH_SIZE])
    cg.set_cpp_standard("gnu++20")
    cg.add_build_flag("-DUSE_ESP32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_build_flag(f"-DUSE_ESP32_VARIANT_{config[CONF_VARIANT]}")
    cg.add_define("ESPHOME_VARIANT", VARIANT_FRIENDLY[config[CONF_VARIANT]])
    cg.add_define(ThreadModel.MULTI_ATOMICS)

    cg.add_platformio_option("lib_ldf_mode", "off")
    cg.add_platformio_option("lib_compat_mode", "strict")

    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])

    if conf[CONF_ADVANCED][CONF_IGNORE_EFUSE_CUSTOM_MAC]:
        cg.add_define("USE_ESP32_IGNORE_EFUSE_CUSTOM_MAC")

    add_extra_script(
        "post",
        "post_build.py",
        os.path.join(os.path.dirname(__file__), "post_build.py.script"),
    )

    freq = config[CONF_CPU_FREQUENCY][:-3]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_platformio_option("framework", "espidf")
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        cg.add_build_flag("-Wno-nonnull-compare")

        cg.add_platformio_option("platform_packages", [conf[CONF_SOURCE]])

        # platformio/toolchain-esp32ulp does not support linux_aarch64 yet and has not been updated for over 2 years
        # This is espressif's own published version which is more up to date.
        cg.add_platformio_option(
            "platform_packages", ["espressif/toolchain-esp32ulp@2.35.0-20220830"]
        )
        add_idf_sdkconfig_option(
            f"CONFIG_ESPTOOLPY_FLASHSIZE_{config[CONF_FLASH_SIZE]}", True
        )
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_SINGLE_APP", False)
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_CUSTOM", True)
        add_idf_sdkconfig_option(
            "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME", "partitions.csv"
        )

        # Increase freertos tick speed from 100Hz to 1kHz so that delay() resolution is 1ms
        add_idf_sdkconfig_option("CONFIG_FREERTOS_HZ", 1000)

        # Setup watchdog
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT", True)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_PANIC", True)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0", False)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1", False)

        # Disable dynamic log level control to save memory
        add_idf_sdkconfig_option("CONFIG_LOG_DYNAMIC_LEVEL_CONTROL", False)

        # Set default CPU frequency
        add_idf_sdkconfig_option(f"CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_{freq}", True)

        # Apply LWIP optimization settings
        advanced = conf[CONF_ADVANCED]
        # DHCP server: only disable if explicitly set to false
        # WiFi component handles its own optimization when AP mode is not used
        if (
            CONF_ENABLE_LWIP_DHCP_SERVER in advanced
            and not advanced[CONF_ENABLE_LWIP_DHCP_SERVER]
        ):
            add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", False)
        if not advanced.get(CONF_ENABLE_LWIP_MDNS_QUERIES, True):
            add_idf_sdkconfig_option("CONFIG_LWIP_DNS_SUPPORT_MDNS_QUERIES", False)
        if not advanced.get(CONF_ENABLE_LWIP_BRIDGE_INTERFACE, False):
            add_idf_sdkconfig_option("CONFIG_LWIP_BRIDGEIF_MAX_PORTS", 0)
        if advanced.get(CONF_EXECUTE_FROM_PSRAM, False):
            add_idf_sdkconfig_option("CONFIG_SPIRAM_FETCH_INSTRUCTIONS", True)
            add_idf_sdkconfig_option("CONFIG_SPIRAM_RODATA", True)

        # Apply LWIP core locking for better socket performance
        # This is already enabled by default in Arduino framework, where it provides
        # significant performance benefits. Our benchmarks show socket operations are
        # 24-200% faster with core locking enabled:
        # - select() on 4 sockets: ~190μs (Arduino/core locking) vs ~235μs (ESP-IDF default)
        # - Up to 200% slower under load when all operations queue through tcpip_thread
        # Enabling this makes ESP-IDF socket performance match Arduino framework.
        if advanced.get(CONF_ENABLE_LWIP_TCPIP_CORE_LOCKING, True):
            add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_CORE_LOCKING", True)
        if advanced.get(CONF_ENABLE_LWIP_CHECK_THREAD_SAFETY, True):
            add_idf_sdkconfig_option("CONFIG_LWIP_CHECK_THREAD_SAFETY", True)

        cg.add_platformio_option("board_build.partitions", "partitions.csv")
        if CONF_PARTITIONS in config:
            add_extra_build_file(
                "partitions.csv", CORE.relative_config_path(config[CONF_PARTITIONS])
            )

        if assertion_level := advanced.get(CONF_ASSERTION_LEVEL):
            for key, flag in ASSERTION_LEVELS.items():
                add_idf_sdkconfig_option(flag, assertion_level == key)

        add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_DEFAULT", False)
        compiler_optimization = advanced.get(CONF_COMPILER_OPTIMIZATION)
        for key, flag in COMPILER_OPTIMIZATIONS.items():
            add_idf_sdkconfig_option(flag, compiler_optimization == key)

        add_idf_sdkconfig_option(
            "CONFIG_LWIP_ESP_LWIP_ASSERT",
            conf[CONF_ADVANCED][CONF_ENABLE_LWIP_ASSERT],
        )

        if advanced.get(CONF_IGNORE_EFUSE_MAC_CRC):
            add_idf_sdkconfig_option("CONFIG_ESP_MAC_IGNORE_MAC_CRC_ERROR", True)
            add_idf_sdkconfig_option(
                "CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE", False
            )
        if advanced.get(CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES):
            _LOGGER.warning(
                "Using experimental features in ESP-IDF may result in unexpected failures."
            )
            add_idf_sdkconfig_option("CONFIG_IDF_EXPERIMENTAL_FEATURES", True)

        cg.add_define(
            "USE_ESP_IDF_VERSION_CODE",
            cg.RawExpression(
                f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
            ),
        )

        for name, value in conf[CONF_SDKCONFIG_OPTIONS].items():
            add_idf_sdkconfig_option(name, RawSdkconfigValue(value))

        for component in conf[CONF_COMPONENTS]:
            add_idf_component(
                name=component[CONF_NAME],
                repo=component.get(CONF_SOURCE),
                ref=component.get(CONF_REF),
                path=component.get(CONF_PATH),
            )
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        cg.add_platformio_option("framework", "arduino")
        cg.add_build_flag("-DUSE_ARDUINO")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ARDUINO")
        cg.add_platformio_option("platform_packages", [conf[CONF_SOURCE]])

        if CONF_PARTITIONS in config:
            cg.add_platformio_option("board_build.partitions", config[CONF_PARTITIONS])
        else:
            cg.add_platformio_option("board_build.partitions", "partitions.csv")

        cg.add_define(
            "USE_ARDUINO_VERSION_CODE",
            cg.RawExpression(
                f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
            ),
        )
        cg.add(RawExpression(f"setCpuFrequencyMhz({freq})"))


APP_PARTITION_SIZES = {
    "2MB": 0x0C0000,  # 768 KB
    "4MB": 0x1C0000,  # 1792 KB
    "8MB": 0x3C0000,  # 3840 KB
    "16MB": 0x7C0000,  # 7936 KB
    "32MB": 0xFC0000,  # 16128 KB
}


def get_arduino_partition_csv(flash_size):
    app_partition_size = APP_PARTITION_SIZES[flash_size]
    eeprom_partition_size = 0x1000  # 4 KB
    spiffs_partition_size = 0xF000  # 60 KB

    app0_partition_start = 0x010000  # 64 KB
    app1_partition_start = app0_partition_start + app_partition_size
    eeprom_partition_start = app1_partition_start + app_partition_size
    spiffs_partition_start = eeprom_partition_start + eeprom_partition_size

    return f"""\
nvs,      data, nvs,     0x9000, 0x5000,
otadata,  data, ota,     0xE000, 0x2000,
app0,     app,  ota_0,   0x{app0_partition_start:X}, 0x{app_partition_size:X},
app1,     app,  ota_1,   0x{app1_partition_start:X}, 0x{app_partition_size:X},
eeprom,   data, 0x99,    0x{eeprom_partition_start:X}, 0x{eeprom_partition_size:X},
spiffs,   data, spiffs,  0x{spiffs_partition_start:X}, 0x{spiffs_partition_size:X}
"""


def get_idf_partition_csv(flash_size):
    app_partition_size = APP_PARTITION_SIZES[flash_size]

    return f"""\
otadata,  data, ota,     ,        0x2000,
phy_init, data, phy,     ,        0x1000,
app0,     app,  ota_0,   ,        0x{app_partition_size:X},
app1,     app,  ota_1,   ,        0x{app_partition_size:X},
nvs,      data, nvs,     ,        0x6D000,
"""


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


def _write_idf_component_yml():
    yml_path = Path(CORE.relative_build_path("src/idf_component.yml"))
    if CORE.data[KEY_ESP32][KEY_COMPONENTS]:
        components: dict = CORE.data[KEY_ESP32][KEY_COMPONENTS]
        dependencies = {}
        for name, component in components.items():
            dependency = {}
            if component[KEY_REF]:
                dependency["version"] = component[KEY_REF]
            if component[KEY_REPO]:
                dependency["git"] = component[KEY_REPO]
            if component[KEY_PATH]:
                dependency["path"] = component[KEY_PATH]
            dependencies[name] = dependency
        contents = yaml_util.dump({"dependencies": dependencies})
    else:
        contents = ""
    write_file_if_changed(yml_path, contents)


# Called by writer.py
def copy_files():
    if (
        CORE.using_arduino
        and "partitions.csv" not in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES]
    ):
        write_file_if_changed(
            CORE.relative_build_path("partitions.csv"),
            get_arduino_partition_csv(
                CORE.platformio_options.get("board_upload.flash_size")
            ),
        )
    if CORE.using_esp_idf:
        _write_sdkconfig()
        _write_idf_component_yml()
        if "partitions.csv" not in CORE.data[KEY_ESP32][KEY_EXTRA_BUILD_FILES]:
            write_file_if_changed(
                CORE.relative_build_path("partitions.csv"),
                get_idf_partition_csv(
                    CORE.platformio_options.get("board_upload.flash_size")
                ),
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
        if file[KEY_PATH].startswith("http"):
            import requests

            mkdir_p(CORE.relative_build_path(os.path.dirname(file[KEY_NAME])))
            with open(CORE.relative_build_path(file[KEY_NAME]), "wb") as f:
                f.write(requests.get(file[KEY_PATH], timeout=30).content)
        else:
            copy_file_if_changed(
                file[KEY_PATH],
                CORE.relative_build_path(file[KEY_NAME]),
            )
