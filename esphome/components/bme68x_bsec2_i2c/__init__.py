import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.bme68x_bsec2 import (
    CONFIG_SCHEMA_BASE,
    BME68xBSEC2Component,
    download_bme68x_blob,
    to_code_base,
    validate_bme68x,
)
import esphome.config_validation as cv

CODEOWNERS = ["@neffs", "@kbx81"]

AUTO_LOAD = ["bme68x_bsec2"]
DEPENDENCIES = ["i2c"]

bme68x_bsec2_i2c_ns = cg.esphome_ns.namespace("bme68x_bsec2_i2c")
BME68xBSEC2I2CComponent = bme68x_bsec2_i2c_ns.class_(
    "BME68xBSEC2I2CComponent", BME68xBSEC2Component, i2c.I2CDevice
)


CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA_BASE.extend(
        cv.Schema({cv.GenerateID(): cv.declare_id(BME68xBSEC2I2CComponent)})
    ).extend(i2c.i2c_device_schema(0x76)),
    cv.only_with_arduino,
    validate_bme68x,
    download_bme68x_blob,
)


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
