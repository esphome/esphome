import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ATTENUATION, CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, global_ns

ATTENUATION_MODES = {
    '0db': global_ns.ADC_0db,
    '2.5db': global_ns.ADC_2_5db,
    '6db': global_ns.ADC_6db,
    '11db': global_ns.ADC_11db,
}


def validate_adc_pin(value):
    vcc = str(value).upper()
    if vcc == 'VCC':
        return cv.only_on_esp8266(vcc)
    return pins.analog_pin(value)


ADCSensorComponent = sensor.sensor_ns.class_('ADCSensorComponent', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ADCSensorComponent),
    vol.Required(CONF_PIN): validate_adc_pin,
    vol.Optional(CONF_ATTENUATION): vol.All(cv.only_on_esp32, cv.one_of(*ATTENUATION_MODES,
                                                                        lower=True)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    pin = config[CONF_PIN]
    if pin == 'VCC':
        pin = 0
    rhs = App.make_adc_sensor(config[CONF_NAME], pin,
                              config.get(CONF_UPDATE_INTERVAL))
    adc = Pvariable(config[CONF_ID], rhs)
    if CONF_ATTENUATION in config:
        add(adc.set_attenuation(ATTENUATION_MODES[config[CONF_ATTENUATION]]))
    sensor.setup_sensor(adc, config)
    setup_component(adc, config)


BUILD_FLAGS = '-DUSE_ADC_SENSOR'


def required_build_flags(config):
    if config[CONF_PIN] == 'VCC':
        return '-DUSE_ADC_SENSOR_VCC'
    return None


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)


def includes(config):
    if config[CONF_PIN] == 'VCC':
        return 'ADC_MODE(ADC_VCC);'
    return None
