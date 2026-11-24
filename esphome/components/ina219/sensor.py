import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_MAX_CURRENT,
    CONF_MAX_VOLTAGE,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
)
from esphome.types import ConfigType

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
                kind=sensor.VOLTAGE, accuracy_decimals=2
            ),
            cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
                kind=sensor.VOLTAGE, accuracy_decimals=2
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                kind=sensor.CURRENT, accuracy_decimals=3
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                kind=sensor.POWER, accuracy_decimals=2
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


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))
    cg.add(var.set_max_voltage_v(config[CONF_MAX_VOLTAGE]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BUS_VOLTAGE, var.set_bus_voltage_sensor)
    await sensors(CONF_SHUNT_VOLTAGE, var.set_shunt_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)
