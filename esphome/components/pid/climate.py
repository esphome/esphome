import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import climate, sensor, output, light
from esphome.const import (
    CONF_COOL_ACTION, CONF_HEAT_ACTION, CONF_ID, CONF_IDLE_ACTION, CONF_SENSOR,
    CONF_OUTPUT, CONF_DEFAULT_TARGET_TEMPERATURE, CONF_TUNING, CONF_KP, CONF_KI,
    CONF_KD, CONF_I_ENABLE, CONF_I_MAX)

bang_bang_ns = cg.esphome_ns.namespace('pid')
PidClimate = bang_bang_ns.class_('PidClimate', climate.Climate, cg.Component)
PidClimateTuningParams = bang_bang_ns.struct('PidClimateTuningParams')

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(PidClimate),
    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE): cv.temperature,
    cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    cv.Required(CONF_TUNING): cv.Schema({
        cv.Required(CONF_KP): cv.float_,
        cv.Required(CONF_KI): cv.float_,
        cv.Required(CONF_KD): cv.float_,
        cv.Required(CONF_I_ENABLE): cv.float_,
        cv.Required(CONF_I_MAX): cv.float_,
    }),
}).extend(cv.polling_component_schema('15s')))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    sens = yield cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))
    cg.add(var.set_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))
    tuning_params = PidClimateTuningParams(
        config[CONF_TUNING][CONF_KP],
        config[CONF_TUNING][CONF_KI],
        config[CONF_TUNING][CONF_KD],
        config[CONF_TUNING][CONF_I_MAX],
        config[CONF_TUNING][CONF_I_ENABLE],
    )
    cg.add(var.set_tuning_params(tuning_params))

    out = yield cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))
