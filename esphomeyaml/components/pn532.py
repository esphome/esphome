import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_CS_PIN, CONF_ID, CONF_SPI_ID, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable, get_variable, gpio_output_pin_expression

DEPENDENCIES = ['spi']

PN532Component = binary_sensor.binary_sensor_ns.PN532Component

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(PN532Component),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})])


def to_code(config):
    for conf in config:
        spi = None
        for spi in get_variable(conf[CONF_SPI_ID]):
            yield
        cs = None
        for cs in gpio_output_pin_expression(conf[CONF_CS_PIN]):
            yield
        rhs = App.make_pn532_component(spi, cs, conf.get(CONF_UPDATE_INTERVAL))
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_PN532'
