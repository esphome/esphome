import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import CONF_AIRTON_SERIAL_ID, AirtonSerialClimate

CONF_ECO = "eco"
CONF_HEALTH = "health"
CONF_IONIZER = "ionizer"
CONF_SLEEP = "sleep"

_SWITCH_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    entity_category=ENTITY_CATEGORY_CONFIG,
    default_restore_mode="DISABLED",
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_SERIAL_ID): cv.use_id(AirtonSerialClimate),
        cv.Optional(CONF_DISPLAY): _SWITCH_SCHEMA,
        cv.Optional(CONF_IONIZER): _SWITCH_SCHEMA,
        cv.Optional(CONF_SLEEP): _SWITCH_SCHEMA,
        cv.Optional(CONF_ECO): _SWITCH_SCHEMA,
        cv.Optional(CONF_HEALTH): _SWITCH_SCHEMA,
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AIRTON_SERIAL_ID])
    switches = switch.sub_switches(config, parent=hub)

    async def flag_switch(key: str, setter: MockObj, flag: str) -> None:
        apply = f"set_flag(airton_serial::{flag}, state)"
        await switches(key, setter, apply=apply, publish="after")

    await flag_switch(CONF_DISPLAY, hub.set_display_switch, "FLAG_DISPLAY")
    await flag_switch(CONF_IONIZER, hub.set_ionizer_switch, "FLAG_IONIZER")
    await flag_switch(CONF_SLEEP, hub.set_sleep_switch, "FLAG_SLEEP")
    await flag_switch(CONF_ECO, hub.set_eco_switch, "FLAG_ECO")
    await flag_switch(CONF_HEALTH, hub.set_health_switch, "FLAG_HEALTH")
