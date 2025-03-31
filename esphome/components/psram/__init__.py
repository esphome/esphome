import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_SPEED
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]

psram_ns = cg.esphome_ns.namespace("psram")
PsramComponent = psram_ns.class_("PsramComponent", cg.Component)

TYPE_QUAD = "quad"
TYPE_OCTAL = "octal"

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
        raise cv.Invalid("PSRAM 120MHz is not supported in octal mode")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PsramComponent),
            cv.Optional(CONF_MODE, default=TYPE_QUAD): cv.enum(
                SPIRAM_MODES, lower=True
            ),
            cv.Optional(CONF_SPEED, default=40e6): cv.All(
                cv.frequency, cv.one_of(*SPIRAM_SPEEDS)
            ),
        }
    ),
    cv.only_on_esp32,
    validate_psram_mode,
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

        add_idf_sdkconfig_option(f"{SPIRAM_MODES[config[CONF_MODE]]}", True)
        add_idf_sdkconfig_option(f"{SPIRAM_SPEEDS[config[CONF_SPEED]]}", True)

    cg.add_define("USE_PSRAM")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
