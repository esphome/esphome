import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.sensor import MQTT_SENSOR_SCHEMA
from esphomeyaml.const import CONF_ADDRESS, CONF_HUMIDITY, CONF_ID, CONF_IIR_FILTER, CONF_NAME, \
    CONF_OVERSAMPLING, CONF_PRESSURE, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL, CONF_GAS_RESISTANCE
from esphomeyaml.helpers import App, RawExpression, add, variable

DEPENDENCIES = ['i2c']

OVERSAMPLING_OPTIONS = {
    'NONE': 'sensor::BME680_OVERSAMPLING_NONE',
    '1X': 'sensor::BME680_OVERSAMPLING_1X',
    '2X': 'sensor::BME680_OVERSAMPLING_2X',
    '4X': 'sensor::BME680_OVERSAMPLING_4X',
    '8X': 'sensor::BME680_OVERSAMPLING_8X',
    '16X': 'sensor::BME680_OVERSAMPLING_16X',
}

IIR_FILTER_OPTIONS = {
    'OFF': 'sensor::BME680_IIR_FILTER_OFF',
    '1X': 'sensor::BME680_IIR_FILTER_1X',
    '3X': 'sensor::BME680_IIR_FILTER_3X',
    '7X': 'sensor::BME680_IIR_FILTER_7X',
    '15X': 'sensor::BME680_IIR_FILTER_15X',
    '31X': 'sensor::BME680_IIR_FILTER_31X',
    '63X': 'sensor::BME680_IIR_FILTER_63X',
    '127X': 'sensor::BME680_IIR_FILTER_127X',
}

BME680_OVERSAMPLING_SENSOR_SCHEMA = MQTT_SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): vol.All(vol.Upper, vol.Any(*OVERSAMPLING_OPTIONS)),
})

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('bme680'): cv.register_variable_id,
    vol.Optional(CONF_ADDRESS, default=0x76): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): BME680_OVERSAMPLING_SENSOR_SCHEMA,
    vol.Required(CONF_PRESSURE): BME680_OVERSAMPLING_SENSOR_SCHEMA,
    vol.Required(CONF_HUMIDITY): BME680_OVERSAMPLING_SENSOR_SCHEMA,
    vol.Required(CONF_GAS_RESISTANCE): MQTT_SENSOR_SCHEMA,
    vol.Optional(CONF_IIR_FILTER): vol.All(vol.Upper, vol.Any(*IIR_FILTER_OPTIONS)),
    # TODO: Heater
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_bme680_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_GAS_RESISTANCE][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable('Application::MakeBME680Sensor', config[CONF_ID], rhs)
    bme680 = make.Pbme680
    if CONF_OVERSAMPLING in config[CONF_TEMPERATURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_TEMPERATURE][CONF_OVERSAMPLING]]
        add(bme680.set_temperature_oversampling(RawExpression(constant)))
    if CONF_OVERSAMPLING in config[CONF_PRESSURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_PRESSURE][CONF_OVERSAMPLING]]
        add(bme680.set_pressure_oversampling(RawExpression(constant)))
    if CONF_OVERSAMPLING in config[CONF_HUMIDITY]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_HUMIDITY][CONF_OVERSAMPLING]]
        add(bme680.set_humidity_oversampling(RawExpression(constant)))
    if CONF_IIR_FILTER in config:
        constant = IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]
        add(bme680.set_iir_filter(RawExpression(constant)))

    sensor.setup_sensor(bme680.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_mqtt_sensor_component(make.Pmqtt_temperature, config[CONF_TEMPERATURE])

    sensor.setup_sensor(bme680.Pget_pressure_sensor(), config[CONF_PRESSURE])
    sensor.setup_mqtt_sensor_component(make.Pmqtt_pressure, config[CONF_PRESSURE])

    sensor.setup_sensor(bme680.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_mqtt_sensor_component(make.Pmqtt_humidity, config[CONF_HUMIDITY])

    sensor.setup_sensor(bme680.Pget_gas_resistance_sensor(), config[CONF_GAS_RESISTANCE])
    sensor.setup_mqtt_sensor_component(make.Pmqtt_gas_resistance, config[CONF_GAS_RESISTANCE])


BUILD_FLAGS = '-DUSE_BME680'
