import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, ESP_PLATFORM_ESP32, CONF_INTERVAL, \
    CONF_DURATION
from esphome.core import coroutine

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
AUTO_LOAD = ['xiaomi_ble']

CONF_ESP32_BLE_ID = 'esp32_ble_id'
CONF_SCAN_PARAMETERS = 'scan_parameters'
CONF_WINDOW = 'window'
CONF_ACTIVE = 'active'
esp32_ble_tracker_ns = cg.esphome_ns.namespace('esp32_ble_tracker')
ESP32BLETracker = esp32_ble_tracker_ns.class_('ESP32BLETracker', cg.Component)
ESPBTDeviceListener = esp32_ble_tracker_ns.class_('ESPBTDeviceListener')


def validate_scan_parameters(config):
    duration = config[CONF_DURATION]
    interval = config[CONF_INTERVAL]
    window = config[CONF_WINDOW]

    if window > interval:
        raise cv.Invalid("Scan window ({}) needs to be smaller than scan interval ({})"
                         "".format(window, interval))

    if interval.total_milliseconds * 3 > duration.total_milliseconds:
        raise cv.Invalid("Scan duration needs to be at least three times the scan interval to"
                         "cover all BLE channels.")

    return config


bt_uuid16_format = 'XXXX'
bt_uuid32_format = 'XXXXXXXX'
bt_uuid128_format = 'XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX'


def bt_uuid(value):
    in_value = cv.string_strict(value)
    value = in_value.upper()

    if len(value) == len(bt_uuid16_format):
        pattern = re.compile("^[A-F|0-9]{4,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                u"Invalid hexadecimal value for 16 bit UUID format: '{}'".format(in_value))
        return value
    if len(value) == len(bt_uuid32_format):
        pattern = re.compile("^[A-F|0-9]{8,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                u"Invalid hexadecimal value for 32 bit UUID format: '{}'".format(in_value))
        return value
    if len(value) == len(bt_uuid128_format):
        pattern = re.compile(
            "^[A-F|0-9]{8,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{4,}-[A-F|0-9]{12,}$")
        if not pattern.match(value):
            raise cv.Invalid(
                u"Invalid hexadecimal value for 128 UUID format: '{}'".format(in_value))
        return value
    raise cv.Invalid(
        u"Service UUID must be in 16 bit '{}', 32 bit '{}', or 128 bit '{}' format".format(
            bt_uuid16_format, bt_uuid32_format, bt_uuid128_format))


def as_hex(value):
    return cg.RawExpression('0x{}ULL'.format(value))


def as_hex_array(value):
    value = value.replace("-", "")
    cpp_array = ['0x{}'.format(part) for part in [value[i:i+2] for i in range(0, len(value), 2)]]
    return cg.RawExpression(
        '(uint8_t*)(const uint8_t[16]){{{}}}'.format(','.join(reversed(cpp_array))))


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32BLETracker),
    cv.Optional(CONF_SCAN_PARAMETERS, default={}): cv.All(cv.Schema({
        cv.Optional(CONF_DURATION, default='5min'): cv.positive_time_period_seconds,
        cv.Optional(CONF_INTERVAL, default='320ms'): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_WINDOW, default='200ms'): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
    }), validate_scan_parameters),

    cv.Optional('scan_interval'): cv.invalid("This option has been removed in 1.14 (Reason: "
                                             "it never had an effect)"),
}).extend(cv.COMPONENT_SCHEMA)

ESP_BLE_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_id(ESP32BLETracker),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    params = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_duration(params[CONF_DURATION]))
    cg.add(var.set_scan_interval(int(params[CONF_INTERVAL].total_milliseconds / 0.625)))
    cg.add(var.set_scan_window(int(params[CONF_WINDOW].total_milliseconds / 0.625)))
    cg.add(var.set_scan_active(params[CONF_ACTIVE]))


@coroutine
def register_ble_device(var, config):
    paren = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_listener(var))
    yield var
