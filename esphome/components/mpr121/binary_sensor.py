import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.components.mpr121.mpr121_const import CONF_TOUCH_THRESHOLD, \
    CONF_RELEASE_THRESHOLD
from . import mpr121_ns, MPR121Component, CONF_MPR121_ID

DEPENDENCIES = ['mpr121']
MPR121Channel = mpr121_ns.class_('MPR121Channel', binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(MPR121Channel),
    cv.GenerateID(CONF_MPR121_ID): cv.use_id(MPR121Component),
    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=11),
    cv.Optional(CONF_TOUCH_THRESHOLD): cv.All(cv.Coerce(int), cv.Range(min=0x05, max=0x30)),
    cv.Optional(CONF_RELEASE_THRESHOLD): cv.All(cv.Coerce(int), cv.Range(min=0x05, max=0x30)),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield binary_sensor.register_binary_sensor(var, config)
    hub = yield cg.get_variable(config[CONF_MPR121_ID])

    hub = yield cg.get_variable(config[CONF_MPR121_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    if CONF_TOUCH_THRESHOLD in config:
        cg.add(var.set_touch_threshold(config[CONF_TOUCH_THRESHOLD]))
    else:
        cg.add(var.set_touch_threshold(hub.get_touch_threshold()))
    if CONF_RELEASE_THRESHOLD in config:
        cg.add(var.set_release_threshold(config[CONF_RELEASE_THRESHOLD]))
    else:
        cg.add(var.set_release_threshold(hub.get_release_threshold()))
    cg.add(hub.register_channel(var))
