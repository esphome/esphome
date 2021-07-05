from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_CLOCK_PIN,
)

hcs301_ns = cg.esphome_ns.namespace("hcs301")
HCS301 = hcs301_ns.class_("HCS301", cg.Component)

CONF_POWER_PIN = "power_pin"
CONF_PWM_PIN = "pwm_pin"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HCS301),
        cv.Required(CONF_POWER_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_PWM_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    pin_power = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
    cg.add(var.set_power_pin(pin_power))
    pin_clock = await cg.gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_clock_pin(pin_clock))
    pin_pwm = await cg.gpio_pin_expression(config[CONF_PWM_PIN])
    cg.add(var.set_pwm_pin(pin_pwm))
