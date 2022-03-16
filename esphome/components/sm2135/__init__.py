import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
)

AUTO_LOAD = ["output"]
CODEOWNERS = ["@BoukeHaarsma23"]

sm2135_ns = cg.esphome_ns.namespace("sm2135")
SM2135 = sm2135_ns.class_("SM2135", cg.Component)

CONF_RGB_CURRENT = "rgb_current"
CONF_CW_CURRENT = "cw_current"

DRIVE_STRENGTHS_CW = {
10,15,20,25,30,40,45,50,55,60
}

DRIVE_STRENGTHS_RGB = {
10,15,20,25,30,40,45
}


MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2135),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RGB_CURRENT): cv.one_of(*DRIVE_STRENGTHS_RGB, int=True),
        cv.Required(CONF_CW_CURRENT): cv.one_of(*DRIVE_STRENGTHS_CW, int=True)
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
    
