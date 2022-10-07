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

DRIVE_STRENGTHS_CW = {
    "10mA": 10,
    "15mA": 15,
    "20mA": 20,
    "25mA": 25,
    "30mA": 30,
    "35mA": 35,
    "40mA": 40,
    "45mA": 45,
    "50mA": 50,
    "55mA": 55,
    "60mA": 60,
}
DRIVE_STRENGTHS_RGB = {
    "10mA": 10,
    "15mA": 15,
    "20mA": 20,
    "25mA": 25,
    "30mA": 30,
    "35mA": 35,
    "40mA": 40,
    "45mA": 45,
}


MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2135),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RGB_CURRENT, "20mA"): cv.enum(DRIVE_STRENGTHS_RGB),
        cv.Optional(CONF_CW_CURRENT, "10mA"): cv.enum(DRIVE_STRENGTHS_CW),
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
