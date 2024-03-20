import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
)

AUTO_LOAD = ["output"]
CODEOWNERS = ["@BoukeHaarsma23", "@matika77", "@dd32"]

sm2135_ns = cg.esphome_ns.namespace("sm2135")
SM2135 = sm2135_ns.class_("SM2135", cg.Component)

CONF_RGB_CURRENT = "rgb_current"
CONF_CW_CURRENT = "cw_current"
CONF_SEPARATE_MODES = "separate_modes"

SM2135Current = sm2135_ns.enum("SM2135Current")

DRIVE_STRENGTHS_CW = {
    "10mA": SM2135Current.SM2135_CURRENT_10MA,
    "15mA": SM2135Current.SM2135_CURRENT_15MA,
    "20mA": SM2135Current.SM2135_CURRENT_20MA,
    "25mA": SM2135Current.SM2135_CURRENT_25MA,
    "30mA": SM2135Current.SM2135_CURRENT_30MA,
    "35mA": SM2135Current.SM2135_CURRENT_35MA,
    "40mA": SM2135Current.SM2135_CURRENT_40MA,
    "45mA": SM2135Current.SM2135_CURRENT_45MA,
    "50mA": SM2135Current.SM2135_CURRENT_50MA,
    "55mA": SM2135Current.SM2135_CURRENT_55MA,
    "60mA": SM2135Current.SM2135_CURRENT_60MA,
}
DRIVE_STRENGTHS_RGB = {
    "10mA": SM2135Current.SM2135_CURRENT_10MA,
    "15mA": SM2135Current.SM2135_CURRENT_15MA,
    "20mA": SM2135Current.SM2135_CURRENT_20MA,
    "25mA": SM2135Current.SM2135_CURRENT_25MA,
    "30mA": SM2135Current.SM2135_CURRENT_30MA,
    "35mA": SM2135Current.SM2135_CURRENT_35MA,
    "40mA": SM2135Current.SM2135_CURRENT_40MA,
    "45mA": SM2135Current.SM2135_CURRENT_45MA,
}


MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2135),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RGB_CURRENT, "20mA"): cv.enum(DRIVE_STRENGTHS_RGB),
        cv.Optional(CONF_CW_CURRENT, "10mA"): cv.enum(DRIVE_STRENGTHS_CW),
        cv.Optional(CONF_SEPARATE_MODES, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
    clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(clock))

    cg.add(var.set_rgb_current(config[CONF_RGB_CURRENT]))
    cg.add(var.set_cw_current(config[CONF_CW_CURRENT]))
    cg.add(var.set_separate_modes(config[CONF_SEPARATE_MODES]))
