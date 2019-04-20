import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import climate, remote_transmitter
from esphome.const import CONF_PIN, CONF_AWAY_CONFIG, CONF_COOL_ACTION, \
    CONF_DEFAULT_TARGET_TEMPERATURE_HIGH, CONF_DEFAULT_TARGET_TEMPERATURE_LOW, CONF_HEAT_ACTION, \
    CONF_ID, CONF_IDLE_ACTION, CONF_NAME, CONF_SENSOR
from esphome import pins

ir_remote_climate_ns = cg.esphome_ns.namespace('climate')
ClimateRemoteTransmitter = ir_remote_climate_ns.class_('ClimateRemoteTransmitter',
                                                       climate.ClimateDevice)

CONF_TRANSMITTER = 'transmitter'
CONF_SUPPORTS_HEAT = 'supports_heat'
CONF_SUPPORTS_COOL = 'supports_cool'
CONF_MODEL = 'model'

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ClimateRemoteTransmitter),
    cv.Required(CONF_TRANSMITTER): cv.use_id(remote_transmitter.RemoteTransmitterComponent),
    cv.Optional(CONF_SUPPORTS_COOL, default=True): cv.boolean,
    cv.Optional(CONF_SUPPORTS_HEAT, default=True): cv.boolean,
    cv.Required(CONF_MODEL): cv.string
}).extend(cv.COMPONENT_SCHEMA))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)

    transmitter = yield cg.get_variable(config[CONF_TRANSMITTER])

    if CONF_SUPPORTS_COOL in config:
        cg.add(var.set_supports_cool(config[CONF_SUPPORTS_COOL]))
    if CONF_SUPPORTS_HEAT in config:
        cg.add(var.set_supports_heat(config[CONF_SUPPORTS_HEAT]))

    cg.add(var.set_transmitter(transmitter))