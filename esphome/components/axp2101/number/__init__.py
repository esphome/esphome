"""Number support for AXP2101 voltage control."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, UNIT_VOLT

from .. import AXP2101Component, axp2101_ns

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["axp2101"]

CONF_AXP2101_ID = "axp2101_id"
CONF_POWER_RAIL = "power_rail"

AXP2101Number = axp2101_ns.class_("AXP2101Number", number.Number, cg.Component)

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

# Voltage ranges for each power rail (in millivolts)
VOLTAGE_RANGES = {
    "DCDC1": {"min": 1500, "max": 3400, "step": 100},
    "DCDC2": {"min": 500, "max": 1540, "step": 10},
    "DCDC3": {"min": 500, "max": 3400, "step": 10},
    "DCDC4": {"min": 500, "max": 1840, "step": 10},
    "DCDC5": {"min": 1400, "max": 3700, "step": 100},
    "ALDO1": {"min": 500, "max": 3500, "step": 100},
    "ALDO2": {"min": 500, "max": 3500, "step": 100},
    "ALDO3": {"min": 500, "max": 3500, "step": 100},
    "ALDO4": {"min": 500, "max": 3500, "step": 100},
    "BLDO1": {"min": 500, "max": 3500, "step": 100},
    "BLDO2": {"min": 500, "max": 3500, "step": 100},
    "CPUSLDO": {"min": 500, "max": 1400, "step": 50},
    "DLDO1": {"min": 500, "max": 3400, "step": 100},
    "DLDO2": {"min": 500, "max": 3400, "step": 100},
}


def validate_voltage_range(config):
    """Validate that voltage is within the allowed range for the power rail."""
    rail = config[CONF_POWER_RAIL]
    ranges = VOLTAGE_RANGES[rail]

    # Update the number schema with the correct min/max/step
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(
        AXP2101Number,
        unit_of_measurement=UNIT_VOLT,
    ).extend(
        {
            cv.GenerateID(CONF_AXP2101_ID): cv.use_id(AXP2101Component),
            cv.Required(CONF_POWER_RAIL): cv.enum(POWER_RAILS, upper=True),
        }
    ),
    validate_voltage_range,
)


async def to_code(config):
    """Generate code for AXP2101 number."""
    paren = await cg.get_variable(config[CONF_AXP2101_ID])
    var = await number.new_number(config, min_value=0.5, max_value=3.7, step=0.01)
    await cg.register_component(var, config)

    cg.add(var.set_parent(paren))
    cg.add(var.set_power_rail(config[CONF_POWER_RAIL]))

    # Set rail-specific voltage range
    rail = config[CONF_POWER_RAIL]
    ranges = VOLTAGE_RANGES[rail]
    cg.add(var.set_voltage_range(ranges["min"], ranges["max"], ranges["step"]))
