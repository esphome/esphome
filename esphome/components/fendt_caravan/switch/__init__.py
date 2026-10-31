import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_PARENT_ID, FendtCaravanHubBase, fendt_caravan_ns

FendtSwitch = fendt_caravan_ns.class_(
    "FendtSwitch",
    switch.Switch,
    cg.Parented.template(FendtCaravanHubBase),
)

CONF_MAIN_SWITCH = "main_switch"
CONF_ALL_LIGHTS = "all_lights"
CONF_FLOOR_HEATER = "floor_heater"
CONF_ALDE_HEATER_GAS = "alde_heater_gas"
CONF_ALDE_HEATER = "alde_heater"
CONF_ALDE_WATER_HEATER = "alde_water_heater"
CONF_ALDE_WATER_HEATER_BOOST = "alde_water_heater_boost"
CONF_FRIDGE_STATUS = "fridge_status"


def _switch_schema(icon: str = cv.UNDEFINED) -> cv.Schema:
    return switch.switch_schema(FendtSwitch).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_MAIN_SWITCH: _switch_schema(icon="mdi:switch"),
        CONF_ALL_LIGHTS: _switch_schema(icon="mdi:lamp"),
        CONF_FLOOR_HEATER: _switch_schema(icon="mdi:heat-wave"),
        CONF_ALDE_HEATER: _switch_schema(icon="mdi:radiator"),
        CONF_ALDE_HEATER_GAS: _switch_schema(icon="mdi:gas-burner"),
        CONF_ALDE_WATER_HEATER: _switch_schema(icon="mdi:water-boiler"),
        CONF_ALDE_WATER_HEATER_BOOST: _switch_schema(icon="mdi:fire"),
        CONF_FRIDGE_STATUS: _switch_schema(icon="mdi:power"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await switch.new_switch(config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
