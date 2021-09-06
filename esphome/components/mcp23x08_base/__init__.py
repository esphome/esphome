import esphome.codegen as cg
from esphome.components import mcp23xxx_base

AUTO_LOAD = ["mcp23xxx_base"]
CODEOWNERS = ["@jesserockz"]

mcp23x08_base_ns = cg.esphome_ns.namespace("mcp23x08_base")
MCP23X08Base = mcp23x08_base_ns.class_("MCP23X08Base", mcp23xxx_base.MCP23XXXBase)
