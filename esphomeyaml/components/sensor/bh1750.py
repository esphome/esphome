import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_MAKE_ID, CONF_NAME, CONF_RESOLUTION, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

BH1750_RESOLUTIONS = {
    4.0: sensor.sensor_ns.BH1750_RESOLUTION_4P0_LX,
    1.0: sensor.sensor_ns.BH1750_RESOLUTION_1P0_LX,
    0.5: sensor.sensor_ns.BH1750_RESOLUTION_0P5_LX,
}

MakeBH1750Sensor = Application.MakeBH1750Sensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeBH1750Sensor),
    vol.Optional(CONF_ADDRESS, default=0x23): cv.i2c_address,
    vol.Optional(CONF_RESOLUTION): vol.All(cv.positive_float, cv.one_of(*BH1750_RESOLUTIONS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    rhs = App.make_bh1750_sensor(config[CONF_NAME], config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make_bh1750 = variable(config[CONF_MAKE_ID], rhs)
    bh1750 = make_bh1750.Pbh1750
    if CONF_RESOLUTION in config:
        add(bh1750.set_resolution(BH1750_RESOLUTIONS[config[CONF_RESOLUTION]]))
    sensor.setup_sensor(bh1750, make_bh1750.Pmqtt, config)


BUILD_FLAGS = '-DUSE_BH1750'
