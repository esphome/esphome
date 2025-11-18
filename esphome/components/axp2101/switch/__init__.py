"""Switch support for AXP2101 power rail control."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import AXP2101Component, axp2101_ns

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = ["axp2101"]

CONF_AXP2101_ID = "axp2101_id"
CONF_POWER_RAIL = "power_rail"

AXP2101Switch = axp2101_ns.class_("AXP2101Switch", switch.Switch, cg.Component)

# Power rail enum matching the PowerRail enum in C++
PowerRail = axp2101_ns.enum("PowerRail")
POWER_RAILS = {
    "DCDC1": PowerRail.DCDC1,
    "DCDC2": PowerRail.DCDC2,
    "DCDC3": PowerRail.DCDC3,
    "DCDC4": PowerRail.DCDC4,
    "DCDC5": PowerRail.DCDC5,
    "ALDO1": PowerRail.ALDO1,
    "ALDO2": PowerRail.ALDO2,
    "ALDO3": PowerRail.ALDO3,
    "ALDO4": PowerRail.ALDO4,
    "BLDO1": PowerRail.BLDO1,
    "BLDO2": PowerRail.BLDO2,
    "CPUSLDO": PowerRail.CPUSLDO,
    "DLDO1": PowerRail.DLDO1,
    "DLDO2": PowerRail.DLDO2,
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
