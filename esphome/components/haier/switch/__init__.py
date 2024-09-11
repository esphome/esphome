import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import switch
from esphome.const import (
    CONF_BEEPER,
    CONF_DISPLAY,
    ENTITY_CATEGORY_CONFIG,
)
from ..climate import (
    CONF_HAIER_ID,
    CONF_PROTOCOL,
    HaierClimateBase,
    haier_ns,
    PROTOCOL_HON,
)

CODEOWNERS = ["@paveldn"]
BeeperSwitch = haier_ns.class_("BeeperSwitch", switch.Switch)
HealthModeSwitch = haier_ns.class_("HealthModeSwitch", switch.Switch)
DisplaySwitch = haier_ns.class_("DisplaySwitch", switch.Switch)
QuietModeSwitch = haier_ns.class_("QuietModeSwitch", switch.Switch)

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
            DisplaySwitch,
            icon=ICON_LED_ON,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        cv.Optional(CONF_HEALTH_MODE): switch.switch_schema(
            HealthModeSwitch,
            icon=ICON_LEAF,
            default_restore_mode="DISABLED",
        ),
        # Beeper switch is only supported for HonClimate
        cv.Optional(CONF_BEEPER): switch.switch_schema(
            BeeperSwitch,
            icon=ICON_VOLUME_HIGH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
        # Quiet mode is only supported for HonClimate
        cv.Optional(CONF_QUIET_MODE): switch.switch_schema(
            QuietModeSwitch,
            icon=ICON_VOLUME_OFF,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


def _final_validate(config):
    full_config = fv.full_config.get()
    for switch_type in [CONF_BEEPER, CONF_QUIET_MODE]:
        # Check switches that are only supported for HonClimate
        if config.get(switch_type):
            climate_path = full_config.get_path_for_id(config[CONF_HAIER_ID])[:-1]
            climate_conf = full_config.get_config_for_path(climate_path)
            protocol_type = climate_conf.get(CONF_PROTOCOL)
            if protocol_type.casefold() != PROTOCOL_HON.casefold():
                raise cv.Invalid(
                    f"{switch_type} switch is only supported for hon climate"
                )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HAIER_ID])

    for switch_type in [CONF_DISPLAY, CONF_HEALTH_MODE, CONF_BEEPER, CONF_QUIET_MODE]:
        if conf := config.get(switch_type):
            sw_var = await switch.new_switch(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw_var))
