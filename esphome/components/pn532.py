import voluptuous as vol

from esphome import automation, pins
from esphome.components import binary_sensor, spi
from esphome.components.spi import SPIComponent
import esphome.config_validation as cv
from esphome.const import CONF_CS_PIN, CONF_ID, CONF_ON_TAG, CONF_SPI_ID, CONF_TRIGGER_ID, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, PollingComponent, Trigger, std_string

DEPENDENCIES = ['spi']
MULTI_CONF = True

PN532Component = binary_sensor.binary_sensor_ns.class_('PN532Component', PollingComponent,
                                                       spi.SPIDevice)
PN532Trigger = binary_sensor.binary_sensor_ns.class_('PN532Trigger', Trigger.template(std_string))

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(PN532Component),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_ON_TAG): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_variable_id(PN532Trigger),
    }),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for spi_ in get_variable(config[CONF_SPI_ID]):
        yield
    for cs in gpio_output_pin_expression(config[CONF_CS_PIN]):
        yield
    rhs = App.make_pn532_component(spi_, cs, config.get(CONF_UPDATE_INTERVAL))
    pn532 = Pvariable(config[CONF_ID], rhs)

    for conf_ in config.get(CONF_ON_TAG, []):
        trigger = Pvariable(conf_[CONF_TRIGGER_ID], pn532.make_trigger())
        automation.build_automation(trigger, std_string, conf_)

    setup_component(pn532, config)


BUILD_FLAGS = '-DUSE_PN532'
