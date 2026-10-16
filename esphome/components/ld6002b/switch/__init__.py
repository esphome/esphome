import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6002BComponent, validate_unique_settings
from ..const import (
    CONF_LD6002B_ID,
    CONF_LOW_POWER,
    CONF_POINT_CLOUD,
    CONF_TARGET_DISPLAY,
)

DEPENDENCIES = ["ld6002b"]

# None of these three carry an inversion. They name what the module is doing, not
# how something is wired to it, so an inverted one would only report the opposite
# of the truth -- and the boot restore, which applies a state nothing reports back,
# is where that would be hardest to spot.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_LOW_POWER): switch.switch_schema(
            switch.ApplySwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_POINT_CLOUD): switch.switch_schema(
            switch.ApplySwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TARGET_DISPLAY): switch.switch_schema(
            switch.ApplySwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
)


def final_validate(config: ConfigType) -> None:
    validate_unique_settings(switch.DOMAIN, config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    control = {"apply": "set_switch_state(entity, state)", "publish": "after"}
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_LOW_POWER, hub.set_low_power_switch, **control)
    await switches(CONF_POINT_CLOUD, hub.set_point_cloud_switch, **control)
    await switches(CONF_TARGET_DISPLAY, hub.set_target_display_switch, **control)
