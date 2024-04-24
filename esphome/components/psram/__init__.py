import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option, get_esp32_variant
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_SPEED,
)

CODEOWNERS = ["@esphome/core"]

psram_ns = cg.esphome_ns.namespace("psram")
PsramComponent = psram_ns.class_("PsramComponent", cg.Component)

SPIRAM_MODES = {
    "quad": "CONFIG_SPIRAM_MODE_QUAD",
    "octal": "CONFIG_SPIRAM_MODE_OCT",
}

SPIRAM_SPEEDS = {
    40e6: "CONFIG_SPIRAM_SPEED_40M",
    80e6: "CONFIG_SPIRAM_SPEED_80M",
    120e6: "CONFIG_SPIRAM_SPEED_120M",
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PsramComponent),
            cv.Optional(CONF_MODE): cv.enum(SPIRAM_MODES, lower=True),
            cv.Optional(CONF_SPEED): cv.All(cv.frequency, cv.one_of(*SPIRAM_SPEEDS)),
        }
    ),
    cv.only_on_esp32,
)


async def to_code(config):
    if CORE.using_arduino:
        cg.add_build_flag("-DBOARD_HAS_PSRAM")

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option(
            f"CONFIG_{get_esp32_variant().upper()}_SPIRAM_SUPPORT", True
        )
        add_idf_sdkconfig_option("CONFIG_SPIRAM", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_IGNORE_NOTFOUND", True)

        if CONF_MODE in config:
            add_idf_sdkconfig_option(f"{SPIRAM_MODES[config[CONF_MODE]]}", True)
        if CONF_SPEED in config:
            add_idf_sdkconfig_option(f"{SPIRAM_SPEEDS[config[CONF_SPEED]]}", True)

    cg.add_define("USE_PSRAM")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
