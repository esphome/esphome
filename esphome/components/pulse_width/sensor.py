import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_PIN, UNIT_SECOND, ICON_TIMER

pulse_width_ns = cg.esphome_ns.namespace('pulse_width')

PulseWidthSensor = pulse_width_ns.class_('PulseWidthSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_SECOND, ICON_TIMER, 3).extend({
    cv.GenerateID(): cv.declare_id(PulseWidthSensor),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                  pins.validate_has_interrupt),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
