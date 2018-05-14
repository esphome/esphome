import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_RESOLUTION, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, RawExpression, add, variable

DEPENDENCIES = ['i2c']

BH1750_RESOLUTIONS = {
    4.0: 'sensor::BH1750_RESOLUTION_4P0_LX',
    1.0: 'sensor::BH1750_RESOLUTION_1P0_LX',
    0.5: 'sensor::BH1750_RESOLUTION_0P5_LX',
}

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('bh1750_sensor'): cv.register_variable_id,
    vol.Optional(CONF_ADDRESS, default=0x23): cv.i2c_address,
    vol.Optional(CONF_RESOLUTION): vol.All(cv.positive_float, vol.Any(*BH1750_RESOLUTIONS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.MQTT_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_bh1750_sensor(config[CONF_NAME], config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make_bh1750 = variable('Application::MakeBH1750Sensor', config[CONF_ID], rhs)
    bh1750 = make_bh1750.Pbh1750
    if CONF_RESOLUTION in config:
        constant = BH1750_RESOLUTIONS[config[CONF_RESOLUTION]]
        add(bh1750.set_resolution(RawExpression(constant)))
    sensor.setup_sensor(bh1750, config)
    sensor.setup_mqtt_sensor_component(make_bh1750.Pmqtt, config)


BUILD_FLAGS = '-DUSE_BH1750'
