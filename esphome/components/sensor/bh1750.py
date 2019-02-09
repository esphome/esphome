import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_RESOLUTION, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

DEPENDENCIES = ['i2c']

BH1750Resolution = sensor.sensor_ns.enum('BH1750Resolution')
BH1750_RESOLUTIONS = {
    4.0: BH1750Resolution.BH1750_RESOLUTION_4P0_LX,
    1.0: BH1750Resolution.BH1750_RESOLUTION_1P0_LX,
    0.5: BH1750Resolution.BH1750_RESOLUTION_0P5_LX,
}

BH1750Sensor = sensor.sensor_ns.class_('BH1750Sensor', sensor.PollingSensorComponent,
                                       i2c.I2CDevice)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BH1750Sensor),
    vol.Optional(CONF_ADDRESS, default=0x23): cv.i2c_address,
    vol.Optional(CONF_RESOLUTION): vol.All(cv.positive_float, cv.one_of(*BH1750_RESOLUTIONS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_bh1750_sensor(config[CONF_NAME], config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    bh1750 = Pvariable(config[CONF_ID], rhs)
    if CONF_RESOLUTION in config:
        add(bh1750.set_resolution(BH1750_RESOLUTIONS[config[CONF_RESOLUTION]]))
    sensor.setup_sensor(bh1750, config)
    setup_component(bh1750, config)


BUILD_FLAGS = '-DUSE_BH1750'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
