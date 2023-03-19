import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import (
	CONF_ID
)

DEPENDENCIES = ["spi"]
MULTI_CONF = True
CODEOWNERS = ["@rsumner"]

CONF_GAIN="gain"
CONF_BOOST="boost"
CONF_OVERSAMPLING="oversampling"
CONF_DITHER="dither"
CONF_PRESCALE="prescale"

mcp3911_ns = cg.esphome_ns.namespace("mcp3911")

GAINS = {
    "x1": mcp3911_ns.enum("Gain").GAINX1,
    "x2": mcp3911_ns.enum("Gain").GAINX2,
    "x4": mcp3911_ns.enum("Gain").GAINX4,
    "x8": mcp3911_ns.enum("Gain").GAINX8,
    "x16": mcp3911_ns.enum("Gain").GAINX16,
    "x32": mcp3911_ns.enum("Gain").GAINX32
}

BOOSTS = {    
    "x1": mcp3911_ns.enum("Boost").BOOSTX1,
    "x2": mcp3911_ns.enum("Boost").BOOSTX2,
    "x0.5": mcp3911_ns.enum("Boost").BOOSTX05,
    "x0.66": mcp3911_ns.enum("Boost").BOOSTX066
}

OVERSAMPLINGS = {
    "32": mcp3911_ns.enum("Oversampling").O32,
    "64": mcp3911_ns.enum("Oversampling").O64,
    "128": mcp3911_ns.enum("Oversampling").O128,
    "256": mcp3911_ns.enum("Oversampling").O256,
    "512": mcp3911_ns.enum("Oversampling").O512,
    "1024": mcp3911_ns.enum("Oversampling").O1024,
    "2048": mcp3911_ns.enum("Oversampling").O2048,
    "4096": mcp3911_ns.enum("Oversampling").O4096
}

DITHERS = {
    "off": mcp3911_ns.enum("Dither").OFF,
    "min": mcp3911_ns.enum("Dither").MIN,
    "med": mcp3911_ns.enum("Dither").MED,
    "max": mcp3911_ns.enum("Dither").MAX
}

PRESCALES = {
    "mclk1": mcp3911_ns.enum("Prescale").MCLK1,
    "mclk2": mcp3911_ns.enum("Prescale").MCLK2,
    "mclk4": mcp3911_ns.enum("Prescale").MCLK4,
    "mclk8": mcp3911_ns.enum("Prescale").MCLK8,
}


MCP3911 = mcp3911_ns.class_("MCP3911", cg.Component, spi.SPIDevice)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MCP3911),
        cv.Optional(CONF_GAIN, default="x1"): cv.enum(GAINS),
	cv.Optional(CONF_BOOST, default="x2"): cv.enum(BOOSTS),
        cv.Optional(CONF_OVERSAMPLING, default="4096"): cv.enum(OVERSAMPLINGS),
        cv.Optional(CONF_DITHER, default="max"): cv.enum(DITHERS),
        cv.Optional(CONF_PRESCALE, default="mclk1"): cv.enum(PRESCALES)

    }
).extend(spi.spi_device_schema(cs_pin_required=False))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_boost(config[CONF_BOOST]))
    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_dither(config[CONF_DITHER]))
    cg.add(var.set_prescale(config[CONF_PRESCALE]))
