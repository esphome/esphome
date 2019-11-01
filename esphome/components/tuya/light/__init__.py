from esphome.components import light
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_OUTPUT_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_GAMMA_CORRECT, \
    CONF_DEFAULT_TRANSITION_LENGTH
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ['tuya']

CONF_DIMMER_DATAPOINT = "dimmer_datapoint"
CONF_SWITCH_DATAPOINT = "switch_datapoint"

TuyaLight = tuya_ns.class_('TuyaLight', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TuyaLight),
    cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
    cv.Required(CONF_DIMMER_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_SWITCH_DATAPOINT): cv.uint8_t,
    cv.Optional(CONF_MIN_VALUE): cv.int_,
    cv.Optional(CONF_MAX_VALUE): cv.int_,

    # Change the default gamma_correct and default transition length settings.
    # The Tuya MCU handles transitions and gamma correction on its own.
    cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
    cv.Optional(CONF_DEFAULT_TRANSITION_LENGTH, default='0s'): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    yield light.register_light(var, config)

    if CONF_DIMMER_DATAPOINT in config:
        cg.add(var.set_dimmer_id(config[CONF_DIMMER_DATAPOINT]))
    if CONF_SWITCH_DATAPOINT in config:
        cg.add(var.set_switch_id(config[CONF_SWITCH_DATAPOINT]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))
