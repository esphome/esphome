from esphome.components import fan
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID, CONF_SWITCH_DATAPOINT
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ['tuya']

CONF_SPEED_DATAPOINT = "speed_datapoint"
CONF_OSCILLATION_DATAPOINT = "oscillation_datapoint"

TuyaFan = tuya_ns.class_('TuyaFan', cg.Component)

CONFIG_SCHEMA = cv.All(fan.FAN_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaFan),
    cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
    cv.Optional(CONF_OSCILLATION_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_SPEED_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(
    CONF_SPEED_DATAPOINT, CONF_SWITCH_DATAPOINT))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)

    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    fan_ = yield fan.create_fan_state(config)
    cg.add(var.set_tuya_parent(paren))
    cg.add(var.set_fan(fan_))

    if CONF_SPEED_DATAPOINT in config:
        cg.add(var.set_speed_id(config[CONF_SPEED_DATAPOINT]))
    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_OSCILLATION_DATAPOINT in config:
        cg.add(var.set_oscillation_id(config[CONF_OSCILLATION_DATAPOINT]))
