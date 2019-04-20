import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import climate, remote_transmitter
from esphome.const import CONF_MODEL, CONF_ID, CONF_NAME
from esphome import pins

climate_ns = cg.esphome_ns.namespace('climate')
ClimateRemoteTransmitter = climate_ns.class_('ClimateRemoteTransmitter',
                                             climate.ClimateDevice)

CONF_TRANSMITTER = 'transmitter'
CONF_SUPPORTS_HEAT = 'supports_heat'
CONF_SUPPORTS_COOL = 'supports_cool'

ClimateRemoteTransmitterModel = climate_ns.enum('ClimateRemoteTransmitterModel')
CLIMATE_REMOTE_TRANSMITTER_MODELS = {
    'COOLIX': ClimateRemoteTransmitterModel.CLIMATE_REMOTE_TRANSMITTER_COOLIX,
    'TCL': ClimateRemoteTransmitterModel.CLIMATE_REMOTE_TRANSMITTER_TCL,
}

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ClimateRemoteTransmitter),
    cv.Required(CONF_TRANSMITTER): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
    cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
    cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
    cv.Required(CONF_MODEL): cv.enum(CLIMATE_REMOTE_TRANSMITTER_MODELS, upper=True, space='_'),
}).extend(cv.COMPONENT_SCHEMA))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    if CONF_SUPPORTS_COOL in config:
        cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    if CONF_SUPPORTS_HEAT in config:
        cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))

    transmitter = yield cg.get_variable(config[CONF_TRANSMITTER])
    cg.add(var.set_transmitter(transmitter))
