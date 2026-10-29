import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .climate import CONF_AIRTON_SERIAL_ID, AirtonSerialClimate

CODEOWNERS = ["@guiguid"]

CONF_IONIZER = "ionizer"
CONF_SLEEP = "sleep"
CONF_ECO = "eco"
CONF_HEALTH = "health"


def _switch_schema():
    return switch.switch_schema(
        switch.ApplySwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        default_restore_mode="DISABLED",
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AIRTON_SERIAL_ID): cv.use_id(AirtonSerialClimate),
        cv.Optional(CONF_DISPLAY): _switch_schema(),
        cv.Optional(CONF_IONIZER): _switch_schema(),
        cv.Optional(CONF_SLEEP): _switch_schema(),
        cv.Optional(CONF_ECO): _switch_schema(),
        cv.Optional(CONF_HEALTH): _switch_schema(),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AIRTON_SERIAL_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_DISPLAY, hub.set_display_switch, apply="set_display_state(state)"
    )
    await switches(
        CONF_IONIZER, hub.set_ionizer_switch, apply="set_ionizer_state(state)"
    )
    await switches(CONF_SLEEP, hub.set_sleep_switch, apply="set_sleep_state(state)")
    await switches(CONF_ECO, hub.set_eco_switch, apply="set_eco_state(state)")
    await switches(CONF_HEALTH, hub.set_health_switch, apply="set_health_state(state)")
