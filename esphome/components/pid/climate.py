import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, output
from esphome.const import CONF_ID, CONF_SENSOR

pid_ns = cg.esphome_ns.namespace('pid')
PIDClimate = pid_ns.class_('PIDClimate', climate.Climate, cg.PollingComponent)

CONF_DEFAULT_TARGET_TEMPERATURE = 'default_target_temperature'

CONF_KP = 'kp'
CONF_KI = 'ki'
CONF_KD = 'kd'
CONF_CONTROL_PARAMETERS = 'control_parameters'
CONF_COOL_OUTPUT = 'cool_output'
CONF_HEAT_OUTPUT = 'heat_output'

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(PIDClimate),
    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE): cv.temperature,
    cv.Optional(CONF_COOL_OUTPUT): cv.use_id(output.FloatOutput),
    cv.Optional(CONF_HEAT_OUTPUT): cv.use_id(output.FloatOutput),
    cv.Required(CONF_CONTROL_PARAMETERS): cv.Schema({
        cv.Required(CONF_KP): cv.float_,
        cv.Optional(CONF_KI, default=0.0): cv.float_,
        cv.Optional(CONF_KD, default=0.0): cv.float_,
    }),
}).extend(cv.polling_component_schema('1s')),
                       cv.has_at_least_one_key(CONF_COOL_OUTPUT, CONF_HEAT_OUTPUT))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    sens = yield cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    if CONF_COOL_OUTPUT in config:
        out = yield cg.get_variable(config[CONF_COOL_OUTPUT])
        cg.add(var.set_cool_output(out))
    if CONF_HEAT_OUTPUT in config:
        out = yield cg.get_variable(config[CONF_HEAT_OUTPUT])
        cg.add(var.set_heat_output(out))
    params = config[CONF_CONTROL_PARAMETERS]
    cg.add(var.set_kp(params[CONF_KP]))
    cg.add(var.set_ki(params[CONF_KI]))
    cg.add(var.set_kd(params[CONF_KD]))
