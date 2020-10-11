from esphome.components import climate
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SWITCH_DATAPOINT
from esphome.components.midea_dongle import midea_dongle_ns, CONF_MIDEA_DONGLE_ID, MideaDongle

AUTO_LOAD = ['climate']
DEPENDENCIES = ['midea_dongle']
CODEOWNERS = ['@dudanov']

CONF_BEEPER = 'beeper'
midea_ac_ns = cg.esphome_ns.namespace('midea_ac')
MideaClimate = midea_ac_ns.class_('MideaClimate', climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MideaClimate),
    cv.GenerateID(CONF_MIDEA_DONGLE_ID): cv.use_id(MideaDongle),
    cv.Optional(CONF_BEEPER, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    paren = yield cg.get_variable(config[CONF_MIDEA_DONGLE_ID])
    cg.add(var.set_midea_dongle_parent(paren))
    cg.add(var.set_beeper_feedback(config[CONF_BEEPER]))
