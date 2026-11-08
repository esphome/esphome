import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_PARENT_ID, MainControlUnitHub

CONF_MAIN_SWITCH = "main_switch"
CONF_ALL_LIGHTS = "all_lights"
CONF_FLOOR_HEATER = "floor_heater"


def _switch_schema(icon: str = cv.UNDEFINED) -> cv.Schema:
    return switch.switch_schema(
        switch.ApplySwitch,
        icon=icon,
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(MainControlUnitHub),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_MAIN_SWITCH: _switch_schema(icon="mdi:switch"),
        CONF_ALL_LIGHTS: _switch_schema(icon="mdi:lamp"),
        CONF_FLOOR_HEATER: _switch_schema(icon="mdi:heat-wave"),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches.create(
        config,
        setter=getattr(hub, f"set_{config[CONF_TYPE]}_switch"),
        apply="write_state(entity, state)",
    )
