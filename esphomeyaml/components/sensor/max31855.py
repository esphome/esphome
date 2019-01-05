import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor, spi
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_CS_PIN, CONF_MAKE_ID, CONF_NAME, CONF_SPI_ID, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.cpp_helpers import gpio_output_pin_expression, setup_component
from esphomeyaml.cpp_generator import get_variable, variable
from esphomeyaml.cpp_types import Application, App

MakeMAX31855Sensor = Application.struct('MakeMAX31855Sensor')
MAX31855Sensor = sensor.sensor_ns.class_('MAX31855Sensor', sensor.PollingSensorComponent,
                                         spi.SPIDevice)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MAX31855Sensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMAX31855Sensor),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for spi_ in get_variable(config[CONF_SPI_ID]):
        yield
    for cs in gpio_output_pin_expression(config[CONF_CS_PIN]):
        yield
    rhs = App.make_max31855_sensor(config[CONF_NAME], spi_, cs,
                                   config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    max31855 = make.Pmax31855
    sensor.setup_sensor(max31855, make.Pmqtt, config)
    setup_component(max31855, config)


BUILD_FLAGS = '-DUSE_MAX31855_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
