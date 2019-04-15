from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL, UNIT_PERCENT, \
    ICON_PERCENT

duty_cycle_ns = cg.esphome_ns.namespace('duty_cycle')
DutyCycleSensor = duty_cycle_ns.class_('DutyCycleSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.sensor_schema(UNIT_PERCENT, ICON_PERCENT, 1).extend({
    cv.GenerateID(): cv.declare_variable_id(DutyCycleSensor),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                  pins.validate_has_interrupt),
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL], pin)
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
