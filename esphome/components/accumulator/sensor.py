import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_RESET_VALUE, CONF_SAVE_ON_VALUE_DELTA, \
    CONF_SAVE_MIN_INTERVAL, CONF_SAVE_MAX_INTERVAL

accumulator_ns = cg.esphome_ns.namespace('accumulator')
AccumulatorSensor = accumulator_ns.class_('AccumulatorSensor', sensor.Sensor, cg.Component)
ResetAction = accumulator_ns.class_('ResetAction', automation.Action)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(AccumulatorSensor),
    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),

    cv.Optional(CONF_RESET_VALUE, default=0.0): cv.float_,
    cv.Optional(CONF_SAVE_ON_VALUE_DELTA, default=0.0): cv.positive_float,
    cv.Optional(CONF_SAVE_MIN_INTERVAL, default='0s'): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_SAVE_MAX_INTERVAL, default='0s'): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    sens = yield cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    if CONF_RESET_VALUE in config:
        cg.add(var.set_reset(True))
        cg.add(var.set_reset_value(config[CONF_RESET_VALUE]))

    cg.add(var.set_save_on_value_delta(config[CONF_SAVE_ON_VALUE_DELTA]))
    cg.add(var.set_save_min_interval(config[CONF_SAVE_MIN_INTERVAL]))
    cg.add(var.set_save_max_interval(config[CONF_SAVE_MAX_INTERVAL]))


@automation.register_action('sensor.accumulator.reset', ResetAction, automation.maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(AccumulatorSensor),
}))
def sensor_accumulator_reset_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)
