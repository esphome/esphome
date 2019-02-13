# coding=utf-8
import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_BUS_VOLTAGE, CONF_CURRENT, CONF_ID, \
    CONF_MAX_CURRENT, CONF_MAX_VOLTAGE, CONF_NAME, CONF_POWER, CONF_SHUNT_RESISTANCE, \
    CONF_SHUNT_VOLTAGE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

INA219Component = sensor.sensor_ns.class_('INA219Component', PollingComponent, i2c.I2CDevice)
INA219VoltageSensor = sensor.sensor_ns.class_('INA219VoltageSensor',
                                              sensor.EmptyPollingParentSensor)
INA219CurrentSensor = sensor.sensor_ns.class_('INA219CurrentSensor',
                                              sensor.EmptyPollingParentSensor)
INA219PowerSensor = sensor.sensor_ns.class_('INA219PowerSensor', sensor.EmptyPollingParentSensor)

SENSOR_KEYS = [CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT,
               CONF_POWER]

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(INA219Component),
    vol.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address,
    vol.Optional(CONF_BUS_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA219VoltageSensor),
    })),
    vol.Optional(CONF_SHUNT_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA219VoltageSensor),
    })),
    vol.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA219CurrentSensor),
    })),
    vol.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA219PowerSensor),
    })),
    vol.Optional(CONF_SHUNT_RESISTANCE, default=0.1): vol.All(cv.resistance,
                                                              vol.Range(min=0.0, max=32.0)),
    vol.Optional(CONF_MAX_VOLTAGE, default=32.0): vol.All(cv.voltage, vol.Range(min=0.0, max=32.0)),
    vol.Optional(CONF_MAX_CURRENT, default=3.2): vol.All(cv.current, vol.Range(min=0.0)),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSOR_KEYS))


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
    setup_component(ina, config)


BUILD_FLAGS = '-DUSE_INA219'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT, CONF_POWER):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
