import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL, UNIT_SECOND, \
    ICON_TIMER

pulse_width_ns = cg.esphome_ns.namespace('pulse_width')

PulseWidthSensor = pulse_width_ns.class_('PulseWidthSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.sensor_schema(UNIT_SECOND, ICON_TIMER, 3).extend({
    cv.GenerateID(): cv.declare_variable_id(PulseWidthSensor),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pullup_pin_schema,
                                  pins.validate_has_interrupt),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
