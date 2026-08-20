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
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await switch.new_switch(config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
