import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_HUMIDITY, CONF_ID, CONF_IIR_FILTER, CONF_NAME, \
    CONF_OVERSAMPLING, CONF_PRESSURE, CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

BME280Oversampling = sensor.sensor_ns.enum('BME280Oversampling')
OVERSAMPLING_OPTIONS = {
    'NONE': BME280Oversampling.BME280_OVERSAMPLING_NONE,
    '1X': BME280Oversampling.BME280_OVERSAMPLING_1X,
    '2X': BME280Oversampling.BME280_OVERSAMPLING_2X,
    '4X': BME280Oversampling.BME280_OVERSAMPLING_4X,
    '8X': BME280Oversampling.BME280_OVERSAMPLING_8X,
    '16X': BME280Oversampling.BME280_OVERSAMPLING_16X,
}

BME280IIRFilter = sensor.sensor_ns.enum('BME280IIRFilter')
IIR_FILTER_OPTIONS = {
    'OFF': BME280IIRFilter.BME280_IIR_FILTER_OFF,
    '2X': BME280IIRFilter.BME280_IIR_FILTER_2X,
    '4X': BME280IIRFilter.BME280_IIR_FILTER_4X,
    '8X': BME280IIRFilter.BME280_IIR_FILTER_8X,
    '16X': BME280IIRFilter.BME280_IIR_FILTER_16X,
}

BME280_OVERSAMPLING_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): cv.one_of(*OVERSAMPLING_OPTIONS, upper=True),
})

BME280Component = sensor.sensor_ns.class_('BME280Component', PollingComponent, i2c.I2CDevice)
BME280TemperatureSensor = sensor.sensor_ns.class_('BME280TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BME280PressureSensor = sensor.sensor_ns.class_('BME280PressureSensor',
                                               sensor.EmptyPollingParentSensor)
BME280HumiditySensor = sensor.sensor_ns.class_('BME280HumiditySensor',
                                               sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BME280Component),
    vol.Optional(CONF_ADDRESS, default=0x77): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME280TemperatureSensor),
    })),
    vol.Required(CONF_PRESSURE): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME280PressureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(BME280_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME280HumiditySensor),
    })),
    vol.Optional(CONF_IIR_FILTER): cv.one_of(*IIR_FILTER_OPTIONS, upper=True),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_bme280_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    bme280 = Pvariable(config[CONF_ID], rhs)
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

    sensor.setup_sensor(bme280.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(bme280.Pget_pressure_sensor(), config[CONF_PRESSURE])
    sensor.setup_sensor(bme280.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    setup_component(bme280, config)


BUILD_FLAGS = '-DUSE_BME280'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY])]
