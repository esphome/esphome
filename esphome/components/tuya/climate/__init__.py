from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SWITCH_DATAPOINT
from esphome.util import Registry
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ['tuya']
CODEOWNERS = ['@jesserockz']

CONF_TARGET_TEMPERATURE_DATAPOINT = 'target_temperature_datapoint'
CONF_CURRENT_TEMPERATURE_DATAPOINT = 'current_temperature_datapoint'
CONF_TEMPERATURE_MULTIPLIER = 'temperature_multiplier'

TuyaClimate = tuya_ns.class_('TuyaClimate', climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TuyaClimate),
    cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
    cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_TARGET_TEMPERATURE_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_CURRENT_TEMPERATURE_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_TEMPERATURE_MULTIPLIER, default=1): cv.positive_float,
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(
    CONF_TARGET_TEMPERATURE_DATAPOINT, CONF_SWITCH_DATAPOINT))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_TARGET_TEMPERATURE_DATAPOINT in config:
        cg.add(var.set_target_temperature_id(config[CONF_TARGET_TEMPERATURE_DATAPOINT]))
    if CONF_CURRENT_TEMPERATURE_DATAPOINT in config:
        cg.add(var.set_current_temperature_id(config[CONF_CURRENT_TEMPERATURE_DATAPOINT]))
    cg.add(var.set_temperature_multiplier(config[CONF_TEMPERATURE_MULTIPLIER]))
