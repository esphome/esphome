from esphome.components import binary_sensor
from esphome.components.ttp229_lsf import TTP229LSFComponent, CONF_TTP229_ID
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CHANNEL, CONF_NAME
DEPENDENCIES = ['ttp229_lsf']
TTP229Channel = binary_sensor.binary_sensor_ns.class_(
    'TTP229Channel', binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TTP229Channel),
    cv.GenerateID(CONF_TTP229_ID): cv.use_variable_id(TTP229LSFComponent),
    cv.Required(CONF_CHANNEL): cv.All(cv.Coerce(int), cv.Range(min=0, max=15))
}))


def to_code(config):
    hub = yield get_variable(config[CONF_TTP229_ID])
    rhs = TTP229Channel.new(config[CONF_NAME], config[CONF_CHANNEL])
    binary_sensor.register_binary_sensor(hub.add_channel(rhs), config)
