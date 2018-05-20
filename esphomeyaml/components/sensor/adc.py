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

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('adc', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.analog_pin,
    vol.Optional(CONF_ATTENUATION): vol.All(cv.only_on_esp32, cv.one_of(*ATTENUATION_MODES)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)

MakeADCSensor = Application.MakeADCSensor


def to_code(config):
    rhs = App.make_adc_sensor(config[CONF_NAME], config[CONF_PIN],
                              config.get(CONF_UPDATE_INTERVAL))
    make = variable(MakeADCSensor, config[CONF_MAKE_ID], rhs)
    adc = make.Padc
    if CONF_ATTENUATION in config:
        add(adc.set_attenuation(ATTENUATION_MODES[config[CONF_ATTENUATION]]))
    sensor.setup_sensor(make.Padc, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ADC_SENSOR'
