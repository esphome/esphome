import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_ID, CONF_SETUP_MODE, CONF_IIR_FILTER, \
    CONF_SLEEP_DURATION, CONF_MEASUREMENT_DURATION, CONF_LOW_VOLTAGE_REFERENCE, \
    CONF_HIGH_VOLTAGE_REFERENCE, CONF_VOLTAGE_ATTENUATION, ESP_PLATFORM_ESP32
from esphomeyaml.core import TimePeriod
from esphomeyaml.helpers import App, Pvariable, add, global_ns

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]


def validate_voltage(values):
    def validator(value):
        if isinstance(value, float) and value.is_integer():
            value = int(value)
        value = cv.string(value)
        if not value.endswith('V'):
            value += 'V'
        return cv.one_of(*values)(value)
    return validator


LOW_VOLTAGE_REFERENCE = {
    '0.5V': global_ns.TOUCH_LVOLT_0V5,
    '0.6V': global_ns.TOUCH_LVOLT_0V6,
    '0.7V': global_ns.TOUCH_LVOLT_0V7,
    '0.8V': global_ns.TOUCH_LVOLT_0V8,
}
HIGH_VOLTAGE_REFERENCE = {
    '2.4V': global_ns.TOUCH_HVOLT_2V4,
    '2.5V': global_ns.TOUCH_HVOLT_2V5,
    '2.6V': global_ns.TOUCH_HVOLT_2V6,
    '2.7V': global_ns.TOUCH_HVOLT_2V7,
}
VOLTAGE_ATTENUATION = {
    '1.5V': global_ns.TOUCH_HVOLT_ATTEN_1V5,
    '1V': global_ns.TOUCH_HVOLT_ATTEN_1V,
    '0.5V': global_ns.TOUCH_HVOLT_ATTEN_0V5,
    '0V': global_ns.TOUCH_HVOLT_ATTEN_0V,
}

ESP32TouchComponent = binary_sensor.binary_sensor_ns.ESP32TouchComponent

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32TouchComponent),
    vol.Optional(CONF_SETUP_MODE): cv.boolean,
    vol.Optional(CONF_IIR_FILTER): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_SLEEP_DURATION):
        vol.All(cv.positive_time_period, vol.Range(max=TimePeriod(microseconds=436906))),
    vol.Optional(CONF_MEASUREMENT_DURATION):
        vol.All(cv.positive_time_period, vol.Range(max=TimePeriod(microseconds=8192))),
    vol.Optional(CONF_LOW_VOLTAGE_REFERENCE): validate_voltage(LOW_VOLTAGE_REFERENCE),
    vol.Optional(CONF_HIGH_VOLTAGE_REFERENCE): validate_voltage(HIGH_VOLTAGE_REFERENCE),
    vol.Optional(CONF_VOLTAGE_ATTENUATION): validate_voltage(VOLTAGE_ATTENUATION),
})


def to_code(config):
    rhs = App.make_esp32_touch_component()
    touch = Pvariable(config[CONF_ID], rhs)
    if CONF_SETUP_MODE in config:
        add(touch.set_setup_mode(config[CONF_SETUP_MODE]))
    if CONF_IIR_FILTER in config:
        add(touch.set_iir_filter(config[CONF_IIR_FILTER]))
    if CONF_SLEEP_DURATION in config:
        sleep_duration = int(config[CONF_SLEEP_DURATION].total_microseconds * 0.15)
        add(touch.set_sleep_duration(sleep_duration))
    if CONF_MEASUREMENT_DURATION in config:
        measurement_duration = int(config[CONF_MEASUREMENT_DURATION].total_microseconds * 0.125)
        add(touch.set_measurement_duration(measurement_duration))
    if CONF_LOW_VOLTAGE_REFERENCE in config:
        value = LOW_VOLTAGE_REFERENCE[config[CONF_LOW_VOLTAGE_REFERENCE]]
        add(touch.set_low_voltage_reference(value))
    if CONF_HIGH_VOLTAGE_REFERENCE in config:
        value = HIGH_VOLTAGE_REFERENCE[config[CONF_HIGH_VOLTAGE_REFERENCE]]
        add(touch.set_high_voltage_reference(value))
    if CONF_VOLTAGE_ATTENUATION in config:
        value = VOLTAGE_ATTENUATION[config[CONF_VOLTAGE_ATTENUATION]]
        add(touch.set_voltage_attenuation(value))


BUILD_FLAGS = '-DUSE_ESP32_TOUCH_BINARY_SENSOR'
