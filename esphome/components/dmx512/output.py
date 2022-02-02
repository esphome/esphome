from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_NUMBER, CONF_PIN
from esphome.components import dmx512
from esphome.core import CORE, coroutine

dmx512_ns = cg.esphome_ns.namespace('dmx512')
DMX512Output = dmx512_ns.class_('DMX512Output', output.FloatOutput, cg.Component)

CONF_CHANNEL = 'channel'
CONF_UNIVERSE_ID = 'universe'

def validate_channel(channel):
    if(channel >= 1 and channel <= 512):
        return True
    else:
        return False

def _declare_type(value):
    if CORE.is_esp32:
        if CORE.using_arduino:
            return cv.use_id(dmx512.DMX512ESP32)(value)
    elif CORE.is_esp8266:
        return cv.use_id(dmx512.DMX512ESP8266)(value)
    raise NotImplementedError

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(DMX512Output),
    cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=512),
    cv.GenerateID(CONF_UNIVERSE_ID): _declare_type,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield output.register_output(var, config)
    
    dmx = yield cg.get_variable(config[CONF_UNIVERSE_ID])
    cg.add(var.set_universe(dmx))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
