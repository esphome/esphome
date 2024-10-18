from esphome import pins
import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_OPTIMISTIC, CONF_PULSE_LENGTH, CONF_WAIT_TIME

from .. import hbridge_ns

HBridgeSwitch = hbridge_ns.class_("HBridgeSwitch", switch.Switch, cg.Component)

CODEOWNERS = ["@dwmw2"]

CONF_OFF_PIN = "off_pin"
CONF_ON_PIN = "on_pin"

CONFIG_SCHEMA = (
    switch.switch_schema(HBridgeSwitch)
    .extend(
        {
            cv.Required(CONF_ON_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_OFF_PIN): pins.gpio_output_pin_schema,
            cv.Optional(
                CONF_PULSE_LENGTH, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_WAIT_TIME): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    on_pin = await cg.gpio_pin_expression(config[CONF_ON_PIN])
    cg.add(var.set_on_pin(on_pin))
    off_pin = await cg.gpio_pin_expression(config[CONF_OFF_PIN])
    cg.add(var.set_off_pin(off_pin))
    cg.add(var.set_pulse_length(config[CONF_PULSE_LENGTH]))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    if wait_time := config.get(CONF_WAIT_TIME):
        cg.add(var.set_wait_time(wait_time))
