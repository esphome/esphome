import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_PIN_A, CONF_PIN_B, CONF_ENABLE_PIN

hbridge_ns = cg.esphome_ns.namespace("hbridge")

HBRIDGE_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_ENABLE_PIN): cv.use_id(output.FloatOutput),
    })