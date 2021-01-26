import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.components import time as time_
from esphome.const import CONF_ID

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ['ds3231']

RTCClock = ds3231_ns.class_('DS3231TimeComponent', time_.RealTimeClock, i2c.I2CDevice)

CONFIG_SCHEMA = time_.TIME_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(RTCClock),
    cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
}).extend(cv.COMPONENT_SCHEMA).extend(i2c.i2c_device_schema(0x68))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield time_.register_time(var, config)
