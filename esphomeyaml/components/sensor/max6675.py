import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_CS_PIN, CONF_MAKE_ID, CONF_NAME, CONF_SPI_ID, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, get_variable, gpio_output_pin_expression, variable

MakeMAX6675Sensor = Application.MakeMAX6675Sensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMAX6675Sensor),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    spi = None
    for spi in get_variable(config[CONF_SPI_ID]):
        yield
    cs = None
    for cs in gpio_output_pin_expression(config[CONF_CS_PIN]):
        yield
    rhs = App.make_max6675_sensor(config[CONF_NAME], spi, cs,
                                  config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Pmax6675, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_MAX6675_SENSOR'
