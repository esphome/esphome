import logging

import esphome.codegen as cg
from esphome.components.esp32 import (
    CONF_CPU_FREQUENCY,
    CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES,
    VARIANT_ESP32,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.esp32.const import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_MODE,
    CONF_SPEED,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    PLATFORM_ESP32,
)
from esphome.core import CORE
import esphome.final_validate as fv

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = [PLATFORM_ESP32]

_LOGGER = logging.getLogger(__name__)

psram_ns = cg.esphome_ns.namespace("psram")
PsramComponent = psram_ns.class_("PsramComponent", cg.Component)

TYPE_QUAD = "quad"
TYPE_OCTAL = "octal"
TYPE_HEX = "hex"

SDK_MODES = {TYPE_QUAD: "QUAD", TYPE_OCTAL: "OCT", TYPE_HEX: "HEX"}

CONF_ENABLE_ECC = "enable_ecc"

SPIRAM_MODES = {
    VARIANT_ESP32: (TYPE_QUAD,),
    VARIANT_ESP32S2: (TYPE_QUAD,),
    VARIANT_ESP32S3: (TYPE_QUAD, TYPE_OCTAL),
    VARIANT_ESP32P4: (TYPE_HEX,),
}


SPIRAM_SPEEDS = {
    VARIANT_ESP32: (40, 80, 120),
    VARIANT_ESP32S2: (40, 80, 120),
    VARIANT_ESP32S3: (40, 80, 120),
    VARIANT_ESP32P4: (20, 100, 200),
}


def validate_psram_mode(config):
    esp32_config = fv.full_config.get()[PLATFORM_ESP32]
    if config[CONF_SPEED] == "120MHZ":
        if esp32_config[CONF_CPU_FREQUENCY] != "240MHZ":
            raise cv.Invalid(
                "PSRAM 120MHz requires 240MHz CPU frequency (set in esp32 component)"
            )
        if config[CONF_MODE] == TYPE_OCTAL:
            if (
                esp32_config[CONF_FRAMEWORK]
                .get(CONF_ADVANCED, {})
                .get(CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES)
            ):
                _LOGGER.warning(
                    "120MHz PSRAM in octal mode is an experimental feature - use at your own risk"
                )
            else:
                raise cv.Invalid("PSRAM 120MHz is not supported in octal mode")
    if config[CONF_MODE] != TYPE_OCTAL and config[CONF_ENABLE_ECC]:
        raise cv.Invalid("ECC is only available in octal mode.")
    if config[CONF_MODE] == TYPE_OCTAL:
        variant = get_esp32_variant()
        if variant != VARIANT_ESP32S3:
            raise cv.Invalid(
                f"Octal PSRAM is only supported on ESP32-S3, not {variant}"
            )
    return config


def get_config_schema(config):
    variant = get_esp32_variant()
    speeds = [f"{s}MHZ" for s in SPIRAM_SPEEDS.get(variant, [])]
    if not speeds:
        return cv.Invalid("PSRAM is not supported on this chip")
    modes = SPIRAM_MODES[variant]
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PsramComponent),
            cv.Optional(CONF_MODE, default=modes[0]): cv.one_of(*modes, lower=True),
            cv.Optional(CONF_ENABLE_ECC, default=False): cv.boolean,
            cv.Optional(CONF_SPEED, default=speeds[0]): cv.one_of(*speeds, upper=True),
        }
    )(config)


CONFIG_SCHEMA = get_config_schema

FINAL_VALIDATE_SCHEMA = validate_psram_mode


async def to_code(config):
    if CORE.using_arduino:
        cg.add_build_flag("-DBOARD_HAS_PSRAM")
        if config[CONF_MODE] == TYPE_OCTAL:
            cg.add_platformio_option("board_build.arduino.memory_type", "qio_opi")

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option(
            f"CONFIG_{get_esp32_variant().upper()}_SPIRAM_SUPPORT", True
        )
        add_idf_sdkconfig_option("CONFIG_SOC_SPIRAM_SUPPORTED", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_IGNORE_NOTFOUND", True)

        add_idf_sdkconfig_option(
            f"CONFIG_SPIRAM_MODE_{SDK_MODES[config[CONF_MODE]]}", True
        )

        # Remove MHz suffix, convert to int
        speed = int(config[CONF_SPEED][:-3])
        add_idf_sdkconfig_option(f"CONFIG_SPIRAM_SPEED_{speed}M", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_SPEED", speed)
        if config[CONF_MODE] == TYPE_OCTAL and speed == 120:
            add_idf_sdkconfig_option("CONFIG_ESPTOOLPY_FLASHFREQ_120M", True)
            add_idf_sdkconfig_option("CONFIG_BOOTLOADER_FLASH_DC_AWARE", True)
            if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(5, 4, 0):
                add_idf_sdkconfig_option(
                    "CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR", True
                )
        if config[CONF_ENABLE_ECC]:
            add_idf_sdkconfig_option("CONFIG_SPIRAM_ECC_ENABLE", True)

    cg.add_define("USE_PSRAM")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
