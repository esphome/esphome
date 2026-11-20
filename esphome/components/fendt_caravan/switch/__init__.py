from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

from .. import CONF_PARENT_ID, DOMAIN, MainControlUnitHub

CONF_MAIN_SWITCH = "main_switch"
CONF_ALL_LIGHTS = "all_lights"
CONF_FLOOR_HEATER = "floor_heater"


def _switch_schema(hub: MockObjClass, icon: str = cv.UNDEFINED) -> cv.Schema:
    return switch.switch_schema(
        switch.ApplySwitch,
        icon=icon,
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(hub),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_MAIN_SWITCH: _switch_schema(MainControlUnitHub, icon="mdi:switch"),
        CONF_ALL_LIGHTS: _switch_schema(MainControlUnitHub, icon="mdi:lamp"),
        CONF_FLOOR_HEATER: _switch_schema(MainControlUnitHub, icon="mdi:heat-wave"),
    }
)


# The hub keeps one pointer per type, so a second entry would take over the first one's updates
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="switch",
    platform=DOMAIN,
    keys=(CONF_PARENT_ID, CONF_TYPE),
    message="Only one fendt_caravan switch of type '{type}' is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    setter = {
        CONF_MAIN_SWITCH: hub.set_main_switch_switch,
        CONF_ALL_LIGHTS: hub.set_all_lights_switch,
        CONF_FLOOR_HEATER: hub.set_floor_heater_switch,
    }[config[CONF_TYPE]]
    switches = switch.sub_switches(config, parent=hub)
    await switches.create(
        config,
        setter=setter,
        apply="write_state(entity, state)",
    )
