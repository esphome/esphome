import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.dfrobot_sen0465_base import (
    DFRobotSEN0465Component,
    dfrobot_sen0465_schema,
    to_code_base,
)
from esphome.types import ConfigType

AUTO_LOAD = ["dfrobot_sen0465_base"]
DEPENDENCIES = ["i2c"]

dfrobot_sen0465_i2c_ns = cg.esphome_ns.namespace("dfrobot_sen0465_i2c")
DFRobotSEN0465I2CComponent = dfrobot_sen0465_i2c_ns.class_(
    "DFRobotSEN0465I2CComponent", DFRobotSEN0465Component, i2c.I2CDevice
)

# 0x77 is the factory setting of the A0/A1 switches according to the DFRobot examples
CONFIG_SCHEMA = dfrobot_sen0465_schema(
    DFRobotSEN0465I2CComponent, i2c.i2c_device_schema(0x77)
)


async def to_code(config: ConfigType) -> None:
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
