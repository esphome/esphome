import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import climate, sensor
from esphome.const import (
    CONF_COOL_ACTION, CONF_HEAT_ACTION, CONF_ID, CONF_IDLE_ACTION, CONF_SENSOR,
    CONF_DEFAULT_TARGET_TEMPERATURE)

bang_bang_ns = cg.esphome_ns.namespace('pid')
PidClimate = bang_bang_ns.class_('PidClimate', climate.Climate, cg.Component)

CONF_PID = 'pid'
CONF_K = 'k'
CONF_KI = 'ki'
CONF_KD = 'kd'

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(PidClimate),
    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_DEFAULT_TARGET_TEMPERATURE): cv.temperature,
    cv.Required(CONF_IDLE_ACTION): automation.validate_automation(single=True),
    cv.Optional(CONF_COOL_ACTION): automation.validate_automation(single=True),
    cv.Optional(CONF_HEAT_ACTION): automation.validate_automation(single=True),
    cv.Required(CONF_PID): cv.Schema({
        cv.Required(CONF_K): cv.float_,
        cv.Required(CONF_KI): cv.float_,
        cv.Required(CONF_KD): cv.float_,
    }),
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_COOL_ACTION,
                                                        CONF_HEAT_ACTION))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    sens = yield cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))

    cg.add(var.set_target_temperature(config[CONF_DEFAULT_TARGET_TEMPERATURE]))

    yield automation.build_automation(var.get_idle_trigger(), [], config[CONF_IDLE_ACTION])

    if CONF_COOL_ACTION in config:
        yield automation.build_automation(var.get_cool_trigger(), [], config[CONF_COOL_ACTION])
        cg.add(var.set_supports_cool(True))
    if CONF_HEAT_ACTION in config:
        yield automation.build_automation(var.get_heat_trigger(), [], config[CONF_HEAT_ACTION])
        cg.add(var.set_supports_heat(True))
