import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ATTENUATION, CONF_ID, CONF_NAME, CONF_PIN, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, RawExpression, add, variable

ATTENUATION_MODES = {
    '0db': 'ADC_0db',
    '2.5db': 'ADC_2_5db',
    '6db': 'ADC_6db',
    '11db': 'ADC_11db',
}

ATTENUATION_MODE_SCHEMA = vol.Any(*list(ATTENUATION_MODES.keys()))

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('adc'): cv.register_variable_id,
    vol.Required(CONF_PIN): pins.analog_pin,
    vol.Optional(CONF_ATTENUATION): vol.All(cv.only_on_esp32, ATTENUATION_MODE_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
}).extend(sensor.MQTT_SENSOR_SCHEMA.schema)


def to_code(config):
    rhs = App.make_adc_sensor(config[CONF_NAME], config[CONF_PIN],
                              config.get(CONF_UPDATE_INTERVAL))
    make = variable('Application::MakeADCSensor', config[CONF_ID], rhs)
    adc = make.Padc
    if CONF_ATTENUATION in config:
        attenuation = ATTENUATION_MODES[config[CONF_ATTENUATION]]
        add(adc.set_attenuation(RawExpression(attenuation)))
    sensor.setup_sensor(adc, config)
    sensor.setup_mqtt_sensor_component(make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ADC_SENSOR'
