import esphome.codegen as cg
from esphome.components import mcp23xxx_base

AUTO_LOAD = ["mcp23xxx_base"]
CODEOWNERS = ["@jesserockz"]

mcp23x17_base_ns = cg.esphome_ns.namespace("mcp23x17_base")
MCP23X17Base = mcp23x17_base_ns.class_("MCP23X17Base", mcp23xxx_base.MCP23XXXBase)
