import logging

import esphome.codegen as cg
from esphome.components.esp32 import (
    CONF_ENABLE_IDF_EXPERIMENTAL_FEATURES,
    VARIANT_ESP32,
    add_idf_sdkconfig_option,
    get_esp32_variant,
    only_on_variant,
)
from esphome.components.esp32.const import VARIANT_ESP32S2, VARIANT_ESP32S3
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

CONF_ENABLE_ECC = "enable_ecc"

SPIRAM_MODES = {
    TYPE_QUAD: "CONFIG_SPIRAM_MODE_QUAD",
    TYPE_OCTAL: "CONFIG_SPIRAM_MODE_OCT",
}

SPIRAM_SPEEDS = {
    40e6: "CONFIG_SPIRAM_SPEED_40M",
    80e6: "CONFIG_SPIRAM_SPEED_80M",
    120e6: "CONFIG_SPIRAM_SPEED_120M",
}


def validate_psram_mode(config):
    if config[CONF_MODE] == TYPE_OCTAL and config[CONF_SPEED] == 120e6:
        esp32_config = fv.full_config.get()[PLATFORM_ESP32]
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


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PsramComponent),
            cv.Optional(CONF_MODE, default=TYPE_QUAD): cv.enum(
                SPIRAM_MODES, lower=True
            ),
            cv.Optional(CONF_ENABLE_ECC, default=False): cv.boolean,
            cv.Optional(CONF_SPEED, default=40e6): cv.All(
                cv.frequency, cv.one_of(*SPIRAM_SPEEDS)
            ),
        }
    ),
    only_on_variant(
        supported=[VARIANT_ESP32, VARIANT_ESP32S3, VARIANT_ESP32S2],
    ),
)

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
        add_idf_sdkconfig_option("CONFIG_SPIRAM", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_IGNORE_NOTFOUND", True)

        add_idf_sdkconfig_option(f"{SPIRAM_MODES[config[CONF_MODE]]}", True)
        add_idf_sdkconfig_option(f"{SPIRAM_SPEEDS[config[CONF_SPEED]]}", True)
        if config[CONF_MODE] == TYPE_OCTAL and config[CONF_SPEED] == 120e6:
            add_idf_sdkconfig_option("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240", True)
            if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(5, 4, 0):
                add_idf_sdkconfig_option(
                    "CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR", True
                )
        if config[CONF_ENABLE_ECC]:
            add_idf_sdkconfig_option("CONFIG_SPIRAM_ECC_ENABLE", True)

    cg.add_define("USE_PSRAM")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
