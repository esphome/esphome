import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, remote_transmitter
from esphome.const import CONF_ID, CONF_NAME

coolix_ns = cg.esphome_ns.namespace('coolix')
CoolixClimate = coolix_ns.class_('CoolixClimate', climate.ClimateDevice)

CONF_TRANSMITTER_ID = 'transmitter_id'
CONF_SUPPORTS_HEAT = 'supports_heat'
CONF_SUPPORTS_COOL = 'supports_cool'

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CoolixClimate),
    cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
    cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
    cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    if CONF_SUPPORTS_COOL in config:
        cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    if CONF_SUPPORTS_HEAT in config:
        cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))

    transmitter = yield cg.get_variable(config[CONF_TRANSMITTER_ID])
    cg.add(var.set_transmitter(transmitter))
