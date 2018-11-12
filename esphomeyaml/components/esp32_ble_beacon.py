import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_SCAN_INTERVAL, CONF_TYPE, CONF_UUID, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, ArrayInitializer, Component, Pvariable, RawExpression, add, \
    esphomelib_ns, setup_component

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

ESP32BLEBeacon = esphomelib_ns.class_('ESP32BLEBeacon', Component)

CONF_MAJOR = 'major'
CONF_MINOR = 'minor'

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32BLEBeacon),
    vol.Required(CONF_TYPE): vol.All(vol.Upper, cv.one_of('IBEACON')),
    vol.Required(CONF_UUID): cv.uuid,
    vol.Optional(CONF_MAJOR): cv.uint16_t,
    vol.Optional(CONF_MINOR): cv.uint16_t,
    vol.Optional(CONF_SCAN_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    uuid = config[CONF_UUID].hex
    uuid_arr = [RawExpression('0x{}'.format(uuid[i:i + 2])) for i in range(0, len(uuid), 2)]
    rhs = App.make_esp32_ble_beacon(ArrayInitializer(*uuid_arr, multiline=False))
    ble = Pvariable(config[CONF_ID], rhs)
    if CONF_MAJOR in config:
        add(ble.set_major(config[CONF_MAJOR]))
    if CONF_MINOR in config:
        add(ble.set_minor(config[CONF_MINOR]))

    setup_component(ble, config)


BUILD_FLAGS = '-DUSE_ESP32_BLE_BEACON'
