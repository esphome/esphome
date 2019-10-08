from esphome.components import light
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID, CONF_MIN_VALUE, CONF_MAX_VALUE
from .. import tuya_ns, CONF_TUYA_ID, TUYA

DEPENDENCIES = ['tuya']

CONF_SWITCH = "switch"
CONF_DIMMER = "dimmer"

TUYALIGHT = tuya_ns.class_('TuyaLight', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TUYALIGHT),
    cv.GenerateID(CONF_TUYA_ID): cv.use_id(TUYA),
    cv.Required(CONF_DIMMER): cv.int_,
    cv.Optional(CONF_SWITCH): cv.int_,
    cv.Optional(CONF_MIN_VALUE): cv.int_,
    cv.Optional(CONF_MAX_VALUE): cv.int_,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    yield light.register_light(var, config)

    if CONF_DIMMER in config:
        cg.add(var.set_dimmer(config[CONF_DIMMER]))
    if CONF_SWITCH in config:
        cg.add(var.set_switch(config[CONF_SWITCH]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
