import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import CONF_TTP229_ID, TTP229LSFComponent, ttp229_lsf_ns

DEPENDENCIES = ['ttp229_lsf']
TTP229Channel = ttp229_lsf_ns.class_('TTP229Channel', binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TTP229Channel),
    cv.GenerateID(CONF_TTP229_ID): cv.use_id(TTP229LSFComponent),
    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    hub = yield cg.get_variable(config[CONF_TTP229_ID])
    cg.add(hub.register_channel(var))
