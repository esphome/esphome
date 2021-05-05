import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import simpleevse, switch
from esphome.const import CONF_ID
from . import simpleevse_ns

DEPENDENCIES = ['simpleevse']
AUTO_LOAD = ['switch']

CONF_SIMPLEEVSE_ID = 'simpleevse_id'

SimpleEvseChargingSwitch = simpleevse_ns.class_("SimpleEvseChargingSwitch")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SimpleEvseChargingSwitch),
    cv.GenerateID(CONF_SIMPLEEVSE_ID): cv.use_id(simpleevse.SimpleEvseComponent),
}).extend(switch.SWITCH_SCHEMA)

def to_code(config):
    evse = yield cg.get_variable(config[CONF_SIMPLEEVSE_ID])
    var = cg.new_Pvariable(config[CONF_ID], evse)
    yield switch.register_switch(var, config)

