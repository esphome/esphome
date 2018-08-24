import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ATTENUATION, CONF_MAKE_ID, CONF_NAME, CONF_PIN, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, global_ns, variable

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


MakeADCSensor = Application.MakeADCSensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeADCSensor),
    vol.Required(CONF_PIN): validate_adc_pin,
    vol.Optional(CONF_ATTENUATION): vol.All(cv.only_on_esp32, cv.one_of(*ATTENUATION_MODES)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    pin = config[CONF_PIN]
    if pin == 'VCC':
        pin = 0
    rhs = App.make_adc_sensor(config[CONF_NAME], pin,
                              config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    adc = make.Padc
    if CONF_ATTENUATION in config:
        add(adc.set_attenuation(ATTENUATION_MODES[config[CONF_ATTENUATION]]))
    sensor.setup_sensor(make.Padc, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ADC_SENSOR'


def required_build_flags(config):
    if config[CONF_PIN] == 'VCC':
        return '-DUSE_ADC_SENSOR_VCC'
    return None
