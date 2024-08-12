import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID, CONF_REFERENCE_VOLTAGE

DEPENDENCIES = ["spi"]
MULTI_CONF = True
CODEOWNERS = ["@rsumner"]

mcp3204_ns = cg.esphome_ns.namespace("mcp3204")
MCP3204 = mcp3204_ns.class_("MCP3204", cg.Component, spi.SPIDevice)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MCP3204),
        cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.voltage,
    }
).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
