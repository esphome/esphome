import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_MAX_CURRENT,
    CONF_MAX_VOLTAGE,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_EMPTY,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
)

DEPENDENCIES = ["i2c"]

ina219_ns = cg.esphome_ns.namespace("ina219")
INA219Component = ina219_ns.class_(
    "INA219Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA219Component),
            cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT, ICON_EMPTY, 2, DEVICE_CLASS_VOLTAGE
            ),
            cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
                UNIT_VOLT, ICON_EMPTY, 2, DEVICE_CLASS_VOLTAGE
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                UNIT_AMPERE, ICON_EMPTY, 3, DEVICE_CLASS_CURRENT
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                UNIT_WATT, ICON_EMPTY, 2, DEVICE_CLASS_POWER
            ),
            cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
                cv.resistance, cv.Range(min=0.0, max=32.0)
            ),
            cv.Optional(CONF_MAX_VOLTAGE, default=32.0): cv.All(
                cv.voltage, cv.Range(min=0.0, max=32.0)
            ),
            cv.Optional(CONF_MAX_CURRENT, default=3.2): cv.All(
                cv.current, cv.Range(min=0.0)
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))
    cg.add(var.set_max_voltage_v(config[CONF_MAX_VOLTAGE]))

    if CONF_BUS_VOLTAGE in config:
        sens = yield sensor.new_sensor(config[CONF_BUS_VOLTAGE])
        cg.add(var.set_bus_voltage_sensor(sens))

    if CONF_SHUNT_VOLTAGE in config:
        sens = yield sensor.new_sensor(config[CONF_SHUNT_VOLTAGE])
        cg.add(var.set_shunt_voltage_sensor(sens))

    if CONF_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))

    if CONF_POWER in config:
        sens = yield sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))
