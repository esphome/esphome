from esphome import config_validation as cv, automation
from esphome import codegen as cg
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_ID, CONF_RED, CONF_VALUE, CONF_WHITE

color_ns = cg.esphome_ns.namespace('color')
ColorComponent = color_ns.class_('Color', cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ColorComponent),
    cv.Optional(CONF_ID): cv.declare_id(ColorComponent),
    cv.Optional(CONF_RED, default=0.0): cv.percentage,
    cv.Optional(CONF_GREEN, default=0.0): cv.percentage,
    cv.Optional(CONF_BLUE, default=0.0): cv.percentage,
    cv.Optional(CONF_WHITE, default=0.0): cv.percentage,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_RED], config[CONF_GREEN], \
                            config[CONF_BLUE], config[CONF_WHITE])
    yield cg.register_component(var, config)
