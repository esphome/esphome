# coding=utf-8
import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_BUS_VOLTAGE, CONF_CURRENT, CONF_ID, CONF_NAME, \
    CONF_POWER, CONF_SHUNT_RESISTANCE, CONF_SHUNT_VOLTAGE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

CONF_CHANNEL_1 = 'channel_1'
CONF_CHANNEL_2 = 'channel_2'
CONF_CHANNEL_3 = 'channel_3'

INA3221Component = sensor.sensor_ns.class_('INA3221Component', PollingComponent, i2c.I2CDevice)
INA3221VoltageSensor = sensor.sensor_ns.class_('INA3221VoltageSensor',
                                               sensor.EmptyPollingParentSensor)
INA3221CurrentSensor = sensor.sensor_ns.class_('INA3221CurrentSensor',
                                               sensor.EmptyPollingParentSensor)
INA3221PowerSensor = sensor.sensor_ns.class_('INA3221PowerSensor', sensor.EmptyPollingParentSensor)

SENSOR_KEYS = [CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT, CONF_POWER]

INA3221_CHANNEL_SCHEMA = vol.All(vol.Schema({
    vol.Optional(CONF_BUS_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA3221VoltageSensor),
    })),
    vol.Optional(CONF_SHUNT_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA3221VoltageSensor),
    })),
    vol.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA3221CurrentSensor),
    })),
    vol.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(INA3221PowerSensor),
    })),
    vol.Optional(CONF_SHUNT_RESISTANCE, default=0.1): vol.All(cv.resistance,
                                                              vol.Range(min=0.0, max=32.0)),
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSOR_KEYS))

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(INA3221Component),
    vol.Optional(CONF_ADDRESS, default=0x40): cv.i2c_address,
    vol.Optional(CONF_CHANNEL_1): INA3221_CHANNEL_SCHEMA,
    vol.Optional(CONF_CHANNEL_2): INA3221_CHANNEL_SCHEMA,
    vol.Optional(CONF_CHANNEL_3): INA3221_CHANNEL_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    rhs = App.make_ina3221(config[CONF_ADDRESS], config.get(CONF_UPDATE_INTERVAL))
    ina = Pvariable(config[CONF_ID], rhs)
    for i, channel in enumerate([CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3]):
        if channel not in config:
            continue
        conf = config[channel]
        if CONF_SHUNT_RESISTANCE in conf:
            add(ina.set_shunt_resistance(i, conf[CONF_SHUNT_RESISTANCE]))
        if CONF_BUS_VOLTAGE in conf:
            c = conf[CONF_BUS_VOLTAGE]
            sensor.register_sensor(ina.Pmake_bus_voltage_sensor(i, c[CONF_NAME]), c)
        if CONF_SHUNT_VOLTAGE in conf:
            c = conf[CONF_SHUNT_VOLTAGE]
            sensor.register_sensor(ina.Pmake_shunt_voltage_sensor(i, c[CONF_NAME]), c)
        if CONF_CURRENT in conf:
            c = conf[CONF_CURRENT]
            sensor.register_sensor(ina.Pmake_current_sensor(i, c[CONF_NAME]), c)
        if CONF_POWER in conf:
            c = conf[CONF_POWER]
            sensor.register_sensor(ina.Pmake_power_sensor(i, c[CONF_NAME]), c)

    setup_component(ina, config)


BUILD_FLAGS = '-DUSE_INA3221'


def to_hass_config(data, config):
    ret = []
    for channel in (CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3):
        if channel not in config:
            continue
        conf = config[channel]
        for key in (CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT, CONF_POWER):
            if key in conf:
                ret.append(sensor.core_to_hass_config(data, conf[key]))
    return ret
