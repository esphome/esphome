from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CHANNEL, CONF_NAME, CONF_ID
from . import ttp229_ns, TTP229LSFComponent, CONF_TTP229_ID

DEPENDENCIES = ['ttp229_lsf']
TTP229Channel = ttp229_ns.class_('TTP229Channel', binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TTP229Channel),
    cv.GenerateID(CONF_TTP229_ID): cv.use_variable_id(TTP229LSFComponent),
    cv.Required(CONF_CHANNEL): cv.All(cv.Coerce(int), cv.Range(min=0, max=15))
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_TTP229_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_CHANNEL])
    yield binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_channel(var))
