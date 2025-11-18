"""Switch support for AXP2101 power rail control."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import AXP2101Component, axp2101_ns

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = ["axp2101"]

CONF_AXP2101_ID = "axp2101_id"
CONF_POWER_RAIL = "power_rail"

AXP2101Switch = axp2101_ns.class_("AXP2101Switch", switch.Switch, cg.Component)

# Power rail options matching the PowerRail enum in C++
POWER_RAILS = {
    "DCDC1": 0,
    "DCDC2": 1,
    "DCDC3": 2,
    "DCDC4": 3,
    "DCDC5": 4,
    "ALDO1": 5,
    "ALDO2": 6,
    "ALDO3": 7,
    "ALDO4": 8,
    "BLDO1": 9,
    "BLDO2": 10,
    "CPUSLDO": 11,
    "DLDO1": 12,
    "DLDO2": 13,
}

CONFIG_SCHEMA = switch.switch_schema(AXP2101Switch).extend(
    {
        cv.GenerateID(CONF_AXP2101_ID): cv.use_id(AXP2101Component),
        cv.Required(CONF_POWER_RAIL): cv.enum(POWER_RAILS, upper=True),
    }
)


async def to_code(config):
    """Generate code for AXP2101 switch."""
    paren = await cg.get_variable(config[CONF_AXP2101_ID])
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(paren))
    cg.add(var.set_power_rail(config[CONF_POWER_RAIL]))
