import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_HUMIDITY, CONF_IIR_FILTER, CONF_MAKE_ID, \
    CONF_NAME, CONF_OVERSAMPLING, CONF_PRESSURE, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

OVERSAMPLING_OPTIONS = {
    'NONE': sensor.sensor_ns.BME280_OVERSAMPLING_NONE,
    '1X': sensor.sensor_ns.BME280_OVERSAMPLING_1X,
    '2X': sensor.sensor_ns.BME280_OVERSAMPLING_2X,
    '4X': sensor.sensor_ns.BME280_OVERSAMPLING_4X,
    '8X': sensor.sensor_ns.BME280_OVERSAMPLING_8X,
    '16X': sensor.sensor_ns.BME280_OVERSAMPLING_16X,
}

IIR_FILTER_OPTIONS = {
    'OFF': sensor.sensor_ns.BME280_IIR_FILTER_OFF,
    '2X': sensor.sensor_ns.BME280_IIR_FILTER_2X,
    '4X': sensor.sensor_ns.BME280_IIR_FILTER_4X,
    '8X': sensor.sensor_ns.BME280_IIR_FILTER_8X,
    '16X': sensor.sensor_ns.BME280_IIR_FILTER_16X,
}

BME280_OVERSAMPLING_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): vol.All(vol.Upper, cv.one_of(*OVERSAMPLING_OPTIONS)),
})

MakeBME280Sensor = Application.MakeBME280Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeBME280Sensor),
    vol.Optional(CONF_ADDRESS, default=0x77): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Required(CONF_PRESSURE): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA),
    vol.Optional(CONF_IIR_FILTER): vol.All(vol.Upper, cv.one_of(*IIR_FILTER_OPTIONS)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    rhs = App.make_bme280_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    bme280 = make.Pbme280
    if CONF_OVERSAMPLING in config[CONF_TEMPERATURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_TEMPERATURE][CONF_OVERSAMPLING]]
        add(bme280.set_temperature_oversampling(constant))
    if CONF_OVERSAMPLING in config[CONF_PRESSURE]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_PRESSURE][CONF_OVERSAMPLING]]
        add(bme280.set_pressure_oversampling(constant))
    if CONF_OVERSAMPLING in config[CONF_HUMIDITY]:
        constant = OVERSAMPLING_OPTIONS[config[CONF_HUMIDITY][CONF_OVERSAMPLING]]
        add(bme280.set_humidity_oversampling(constant))
    if CONF_IIR_FILTER in config:
        constant = IIR_FILTER_OPTIONS[config[CONF_IIR_FILTER]]
        add(bme280.set_iir_filter(constant))

    sensor.setup_sensor(bme280.Pget_temperature_sensor(), make.Pmqtt_temperature,
                        config[CONF_TEMPERATURE])
    sensor.setup_sensor(bme280.Pget_pressure_sensor(), make.Pmqtt_pressure,
                        config[CONF_PRESSURE])
    sensor.setup_sensor(bme280.Pget_humidity_sensor(), make.Pmqtt_humidity,
                        config[CONF_HUMIDITY])


BUILD_FLAGS = '-DUSE_BME280'
