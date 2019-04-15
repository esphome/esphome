from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CHANNEL, CONF_NAME, CONF_ID
from . import mpr121_ns, MPR121Component, CONF_MPR121_ID

DEPENDENCIES = ['mpr121']
MPR121Channel = mpr121_ns.class_('MPR121Channel', binary_sensor.BinarySensor)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MPR121Channel),
    cv.GenerateID(CONF_MPR121_ID): cv.use_variable_id(MPR121Component),
    cv.Required(CONF_CHANNEL): cv.All(cv.int_, cv.Range(min=0, max=11))
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_MPR121_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_CHANNEL])
    yield binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_channel(var))
