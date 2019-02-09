import voluptuous as vol

from esphome import config_validation as cv
from esphome.const import CONF_ID, CONF_SCAN_INTERVAL, CONF_TYPE, CONF_UUID, ESP_PLATFORM_ESP32
from esphome.cpp_generator import Pvariable, RawExpression, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component, esphome_ns

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

ESP32BLEBeacon = esphome_ns.class_('ESP32BLEBeacon', Component)

CONF_MAJOR = 'major'
CONF_MINOR = 'minor'

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32BLEBeacon),
    vol.Required(CONF_TYPE): cv.one_of('IBEACON', upper=True),
    vol.Required(CONF_UUID): cv.uuid,
    vol.Optional(CONF_MAJOR): cv.uint16_t,
    vol.Optional(CONF_MINOR): cv.uint16_t,
    vol.Optional(CONF_SCAN_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    uuid = config[CONF_UUID].hex
    uuid_arr = [RawExpression('0x{}'.format(uuid[i:i + 2])) for i in range(0, len(uuid), 2)]
    rhs = App.make_esp32_ble_beacon(uuid_arr)
    ble = Pvariable(config[CONF_ID], rhs)
    if CONF_MAJOR in config:
        add(ble.set_major(config[CONF_MAJOR]))
    if CONF_MINOR in config:
        add(ble.set_minor(config[CONF_MINOR]))

    setup_component(ble, config)


BUILD_FLAGS = '-DUSE_ESP32_BLE_BEACON'
