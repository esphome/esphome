import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    UNIT_EMPTY,
    CONF_PIN,
    CONF_DEBOUNCE,
    CONF_TIMEOUT,
    CONF_THRESHOLD,
    CONF_FREQUENCY,
)

CODEOWNERS = ["@floydheld", "@mathertel"]

multi_button_ns = cg.esphome_ns.namespace('multi_button')
MultiButton = multi_button_ns.class_('MultiButton', cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_EMPTY, "mdi:gesture-tap", 1).extend(
    {
        cv.GenerateID(): cv.declare_id(MultiButton),
        cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DEBOUNCE, default=40): cv.int_range(min=1, max=500),
        cv.Optional(CONF_TIMEOUT, default=300): cv.int_range(min=100, max=5000),
        cv.Optional(CONF_THRESHOLD, default=600): cv.int_range(min=250, max=5000),
        cv.Optional(CONF_FREQUENCY, default=1.5): cv.float_range(min=0.1, max=50),
	}
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_debounce(config[CONF_DEBOUNCE]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_press_hold_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_press_hold_update_rate(config[CONF_FREQUENCY]))
    cg.add(var.set_accuracy_decimals(0))
    