import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins, automation
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_CS_PIN, CONF_ID, CONF_SPI_ID, CONF_UPDATE_INTERVAL, \
    CONF_ON_TAG, CONF_TRIGGER_ID
from esphomeyaml.helpers import App, Pvariable, get_variable, gpio_output_pin_expression, std_string

DEPENDENCIES = ['spi']

PN532Component = binary_sensor.binary_sensor_ns.PN532Component
PN532Trigger = binary_sensor.binary_sensor_ns.PN532Trigger

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(PN532Component),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(PN532Trigger),
    }),
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
        pn532 = Pvariable(conf[CONF_ID], rhs)

        for conf_ in conf.get(CONF_ON_TAG, []):
            trigger = Pvariable(conf_[CONF_TRIGGER_ID], pn532.make_trigger())
            automation.build_automation(trigger, std_string, conf_)


BUILD_FLAGS = '-DUSE_PN532'
