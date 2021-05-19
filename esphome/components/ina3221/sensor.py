import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    ICON_EMPTY,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
)

DEPENDENCIES = ["i2c"]

CONF_CHANNEL_1 = "channel_1"
CONF_CHANNEL_2 = "channel_2"
CONF_CHANNEL_3 = "channel_3"

ina3221_ns = cg.esphome_ns.namespace("ina3221")
INA3221Component = ina3221_ns.class_(
    "INA3221Component", cg.PollingComponent, i2c.I2CDevice
)

INA3221_CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 2, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 2, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 2, DEVICE_CLASS_CURRENT
        ),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 2, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
            cv.resistance, cv.Range(min=0.0, max=32.0)
        ),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA3221Component),
            cv.Optional(CONF_CHANNEL_1): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_2): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_3): INA3221_CHANNEL_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    for i, channel in enumerate([CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3]):
        if channel not in config:
            continue
        conf = config[channel]
        if CONF_SHUNT_RESISTANCE in conf:
            cg.add(var.set_shunt_resistance(i, conf[CONF_SHUNT_RESISTANCE]))
        if CONF_BUS_VOLTAGE in conf:
            sens = yield sensor.new_sensor(conf[CONF_BUS_VOLTAGE])
            cg.add(var.set_bus_voltage_sensor(i, sens))
        if CONF_SHUNT_VOLTAGE in conf:
            sens = yield sensor.new_sensor(conf[CONF_SHUNT_VOLTAGE])
            cg.add(var.set_shunt_voltage_sensor(i, sens))
        if CONF_CURRENT in conf:
            sens = yield sensor.new_sensor(conf[CONF_CURRENT])
            cg.add(var.set_current_sensor(i, sens))
        if CONF_POWER in conf:
            sens = yield sensor.new_sensor(conf[CONF_POWER])
            cg.add(var.set_power_sensor(i, sens))
