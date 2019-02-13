import voluptuous as vol

from esphome import core
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_DURATION, CONF_GAS_RESISTANCE, CONF_HEATER, \
    CONF_HUMIDITY, CONF_ID, CONF_IIR_FILTER, CONF_NAME, CONF_OVERSAMPLING, CONF_PRESSURE, \
    CONF_TEMPERATURE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

BME680Oversampling = sensor.sensor_ns.enum('BME680Oversampling')
OVERSAMPLING_OPTIONS = {
    'NONE': BME680Oversampling.BME680_OVERSAMPLING_NONE,
    '1X': BME680Oversampling.BME680_OVERSAMPLING_1X,
    '2X': BME680Oversampling.BME680_OVERSAMPLING_2X,
    '4X': BME680Oversampling.BME680_OVERSAMPLING_4X,
    '8X': BME680Oversampling.BME680_OVERSAMPLING_8X,
    '16X': BME680Oversampling.BME680_OVERSAMPLING_16X,
}

BME680IIRFilter = sensor.sensor_ns.enum('BME680IIRFilter')
IIR_FILTER_OPTIONS = {
    'OFF': BME680IIRFilter.BME680_IIR_FILTER_OFF,
    '1X': BME680IIRFilter.BME680_IIR_FILTER_1X,
    '3X': BME680IIRFilter.BME680_IIR_FILTER_3X,
    '7X': BME680IIRFilter.BME680_IIR_FILTER_7X,
    '15X': BME680IIRFilter.BME680_IIR_FILTER_15X,
    '31X': BME680IIRFilter.BME680_IIR_FILTER_31X,
    '63X': BME680IIRFilter.BME680_IIR_FILTER_63X,
    '127X': BME680IIRFilter.BME680_IIR_FILTER_127X,
}

BME680_OVERSAMPLING_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    vol.Optional(CONF_OVERSAMPLING): cv.one_of(*OVERSAMPLING_OPTIONS, upper=True),
})

BME680Component = sensor.sensor_ns.class_('BME680Component', PollingComponent, i2c.I2CDevice)
BME680TemperatureSensor = sensor.sensor_ns.class_('BME680TemperatureSensor',
                                                  sensor.EmptyPollingParentSensor)
BME680PressureSensor = sensor.sensor_ns.class_('BME680PressureSensor',
                                               sensor.EmptyPollingParentSensor)
BME680HumiditySensor = sensor.sensor_ns.class_('BME680HumiditySensor',
                                               sensor.EmptyPollingParentSensor)
BME680GasResistanceSensor = sensor.sensor_ns.class_('BME680GasResistanceSensor',
                                                    sensor.EmptyPollingParentSensor)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BME680Component),
    vol.Optional(CONF_ADDRESS, default=0x76): cv.i2c_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME680TemperatureSensor),
    })),
    vol.Required(CONF_PRESSURE): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME680PressureSensor),
    })),
    vol.Required(CONF_HUMIDITY): cv.nameable(BME680_OVERSAMPLING_SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME680HumiditySensor),
    })),
    vol.Required(CONF_GAS_RESISTANCE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(BME680GasResistanceSensor),
    })),
    vol.Optional(CONF_IIR_FILTER): cv.one_of(*IIR_FILTER_OPTIONS, upper=True),
    vol.Optional(CONF_HEATER): vol.Any(None, vol.All(vol.Schema({
        vol.Optional(CONF_TEMPERATURE, default=320): vol.All(vol.Coerce(int), vol.Range(200, 400)),
        vol.Optional(CONF_DURATION, default='150ms'): vol.All(
            cv.positive_time_period_milliseconds, vol.Range(max=core.TimePeriod(milliseconds=4032)))
    }, cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_DURATION)))),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_bme680_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                 config[CONF_PRESSURE][CONF_NAME],
                                 config[CONF_HUMIDITY][CONF_NAME],
                                 config[CONF_GAS_RESISTANCE][CONF_NAME],
                                 config[CONF_ADDRESS],
                                 config.get(CONF_UPDATE_INTERVAL))
    bme680 = Pvariable(config[CONF_ID], rhs)
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

    sensor.setup_sensor(bme680.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.setup_sensor(bme680.Pget_pressure_sensor(), config[CONF_PRESSURE])
    sensor.setup_sensor(bme680.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    sensor.setup_sensor(bme680.Pget_gas_resistance_sensor(), config[CONF_GAS_RESISTANCE])
    setup_component(bme680, config)


BUILD_FLAGS = '-DUSE_BME680'


def to_hass_config(data, config):
    return [sensor.core_to_hass_config(data, config[CONF_TEMPERATURE]),
            sensor.core_to_hass_config(data, config[CONF_PRESSURE]),
            sensor.core_to_hass_config(data, config[CONF_HUMIDITY]),
            sensor.core_to_hass_config(data, config[CONF_GAS_RESISTANCE])]
