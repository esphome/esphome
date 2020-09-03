from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_ID, CONF_RED, CONF_WHITE

ColorStruct = cg.esphome_ns.struct('Color')

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(ColorStruct),
    cv.Optional(CONF_RED, default=0.0): cv.percentage,
    cv.Optional(CONF_GREEN, default=0.0): cv.percentage,
    cv.Optional(CONF_BLUE, default=0.0): cv.percentage,
    cv.Optional(CONF_WHITE, default=0.0): cv.percentage,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    cg.variable(config[CONF_ID], cg.StructInitializer(
        ColorStruct,
        ('r', config[CONF_RED]),
        ('g', config[CONF_GREEN]),
        ('b', config[CONF_BLUE]),
        ('w', config[CONF_WHITE])))
