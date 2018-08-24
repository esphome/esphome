# coding=utf-8
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_CURRENT, CONF_ID, CONF_MAX_CURRENT, \
    CONF_MAX_VOLTAGE, CONF_NAME, CONF_POWER, CONF_UPDATE_INTERVAL, CONF_BUS_VOLTAGE, \
    CONF_SHUNT_VOLTAGE, CONF_SHUNT_RESISTANCE
from esphomeyaml.helpers import App, Pvariable

DEPENDENCIES = ['i2c']

INA219Component = sensor.sensor_ns.INA219Component
INA219VoltageSensor = sensor.sensor_ns.INA219VoltageSensor
INA219CurrentSensor = sensor.sensor_ns.INA219CurrentSensor
INA219PowerSensor = sensor.sensor_ns.INA219PowerSensor

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(INA219Component),
    vol.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address,
    vol.Optional(CONF_BUS_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_SHUNT_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_SHUNT_RESISTANCE, default=0.1): vol.All(cv.resistance,
                                                              vol.Range(min=0.0, max=32.0)),
    vol.Optional(CONF_MAX_VOLTAGE, default=32.0): vol.All(cv.voltage, vol.Range(min=0.0, max=32.0)),
    vol.Optional(CONF_MAX_CURRENT, default=3.2): vol.All(cv.current, vol.Range(min=0.0)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}), cv.has_at_least_one_key(CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT,
                            CONF_POWER))


def to_code(config):
    rhs = App.make_ina219(config[CONF_SHUNT_RESISTANCE],
                          config[CONF_MAX_CURRENT], config[CONF_MAX_VOLTAGE],
                          config[CONF_ADDRESS], config.get(CONF_UPDATE_INTERVAL))
    ina = Pvariable(config[CONF_ID], rhs)
    if CONF_BUS_VOLTAGE in config:
        conf = config[CONF_BUS_VOLTAGE]
        sensor.register_sensor(ina.Pmake_bus_voltage_sensor(conf[CONF_NAME]), conf)
    if CONF_SHUNT_VOLTAGE in config:
        conf = config[CONF_SHUNT_VOLTAGE]
        sensor.register_sensor(ina.Pmake_shunt_voltage_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sensor.register_sensor(ina.Pmake_current_sensor(conf[CONF_NAME]), conf)
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sensor.register_sensor(ina.Pmake_power_sensor(conf[CONF_NAME]), conf)


BUILD_FLAGS = '-DUSE_INA219'
