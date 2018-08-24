import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_GAS_RESISTANCE, CONF_HUMIDITY, CONF_IIR_FILTER, \
    CONF_MAKE_ID, CONF_NAME, CONF_OVERSAMPLING, CONF_PRESSURE, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL, CONF_HEATER, CONF_DURATION
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

OVERSAMPLING_OPTIONS = {
    'NONE': sensor.sensor_ns.BME680_OVERSAMPLING_NONE,
    '1X': sensor.sensor_ns.BME680_OVERSAMPLING_1X,
    '2X': sensor.sensor_ns.BME680_OVERSAMPLING_2X,
    '4X': sensor.sensor_ns.BME680_OVERSAMPLING_4X,
    '8X': sensor.sensor_ns.BME680_OVERSAMPLING_8X,
    '16X': sensor.sensor_ns.BME680_OVERSAMPLING_16X,
}

IIR_FILTER_OPTIONS = {
    'OFF': sensor.sensor_ns.BME680_IIR_FILTER_OFF,
    '1X': sensor.sensor_ns.BME680_IIR_FILTER_1X,
    '3X': sensor.sensor_ns.BME680_IIR_FILTER_3X,
    '7X': sensor.sensor_ns.BME680_IIR_FILTER_7X,
    '15X': sensor.sensor_ns.BME680_IIR_FILTER_15X,
    '31X': sensor.sensor_ns.BME680_IIR_FILTER_31X,
    '63X': sensor.sensor_ns.BME680_IIR_FILTER_63X,
    '127X': sensor.sensor_ns.BME680_IIR_FILTER_127X,
}

BME680_OVERSAMPLING_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): vol.All(vol.Upper, cv.one_of(*OVERSAMPLING_OPTIONS)),
})

MakeBME680Sensor = Application.MakeBME680Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeBME680Sensor),
    vol.Optional(CONF_ADDRESS, default=0x76): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Required(CONF_PRESSURE): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Required(CONF_GAS_RESISTANCE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_IIR_FILTER): vol.All(vol.Upper, cv.one_of(*IIR_FILTER_OPTIONS)),
    vol.Optional(CONF_HEATER): vol.Any(None, vol.All(vol.Schema({
        vol.Optional(CONF_TEMPERATURE, default=320): vol.All(vol.Coerce(int), vol.Range(200, 400)),
        vol.Optional(CONF_DURATION, default='150ms'): vol.All(
            cv.positive_time_period_milliseconds, vol.Range(max=core.TimePeriod(milliseconds=4032)))
    }, cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_DURATION)))),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    rhs = App.make_bme680_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_GAS_RESISTANCE][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    bme680 = make.Pbme680
    if CONF_OVERSAMPLING in config[CONF_TEMPERATURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_TEMPERATURE][CONF_OVERSAMPLING]]
        add(bme680.set_temperature_oversampling(constant))
    if CONF_OVERSAMPLING in config[CONF_PRESSURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_PRESSURE][CONF_OVERSAMPLING]]
        add(bme680.set_pressure_oversampling(constant))
    if CONF_OVERSAMPLING in config[CONF_HUMIDITY]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_HUMIDITY][CONF_OVERSAMPLING]]
        add(bme680.set_humidity_oversampling(constant))
    if CONF_IIR_FILTER in config:
        constant = IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]
        add(bme680.set_iir_filter(constant))
    if CONF_HEATER in config:
        conf = config[CONF_HEATER]
        if not conf:
            add(bme680.set_heater(0, 0))
        else:
            add(bme680.set_heater(conf[CONF_TEMPERATURE], conf[CONF_DURATION]))

    sensor.setup_sensor(bme680.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(bme680.Pget_pressure_sensor(), make.Pmqtt_pressure,
                        config[CONF_PRESSURE])
    sensor.setup_sensor(bme680.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])
    sensor.setup_sensor(bme680.Pget_gas_resistance_sensor(), make.Pmqtt_gas_resistance,
                        config[CONF_GAS_RESISTANCE])


BUILD_FLAGS = '-DUSE_BME680'
