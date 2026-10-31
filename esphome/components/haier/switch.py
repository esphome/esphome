import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_BEEPER, CONF_DISPLAY, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from ..climate import CONF_HAIER_ID, CONF_PROTOCOL, PROTOCOL_HON, HaierClimateBase

CODEOWNERS = ["@paveldn"]

# Haier switches
CONF_HEALTH_MODE = "health_mode"
CONF_QUIET_MODE = "quiet_mode"

# Additional icons
ICON_LEAF = "mdi:leaf"
ICON_LED_ON = "mdi:led-on"
ICON_VOLUME_HIGH = "mdi:volume-high"
ICON_VOLUME_OFF = "mdi:volume-off"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_ID): cv.use_id(HaierClimateBase),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_LEAF,
            default_restore_mode="DISABLED",
        ),
        # Beeper switch is only supported for HonClimate
        cv.Optional(CONF_BEEPER): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_VOLUME_HIGH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        # Quiet mode is only supported for HonClimate
        cv.Optional(CONF_QUIET_MODE): switch.switch_schema(
            switch.ApplySwitch,
            icon=ICON_VOLUME_OFF,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


def _final_validate(config) -> None:
    for switch_type in [CONF_BEEPER, CONF_QUIET_MODE]:
        # Check switches that are only supported for HonClimate
        if config.get(switch_type):
            climate_conf = fv.get_declaring_config(config[CONF_HAIER_ID])
            protocol_type = climate_conf.get(CONF_PROTOCOL)
            if protocol_type.casefold() != PROTOCOL_HON.casefold():
                raise cv.Invalid(
                    f"{switch_type} switch is only supported for hon climate"
                )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The setters skip an unchanged value; the switch reports the state either way
    await switches(
        CONF_DISPLAY,
        hub.set_display_switch,
        apply="set_display_state(state)",
        publish="after",
    )
    await switches(
        CONF_HEALTH_MODE,
        hub.set_health_mode_switch,
        apply="set_health_mode(state)",
        publish="after",
    )
    await switches(
        CONF_BEEPER,
        hub.set_beeper_switch,
        apply="set_beeper_state(state)",
        publish="after",
    )
    await switches(
        CONF_QUIET_MODE,
        hub.set_quiet_mode_switch,
        apply="set_quiet_mode_state(state)",
        publish="after",
    )
