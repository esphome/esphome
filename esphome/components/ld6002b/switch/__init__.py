import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6002BComponent, ld6002b_ns
from ..const import (
    CONF_LD6002B_ID,
    CONF_LOW_POWER,
    CONF_POINT_CLOUD,
    CONF_TARGET_DISPLAY,
)

DEPENDENCIES = ["ld6002b"]

LD6002BSwitch = ld6002b_ns.class_("LD6002BSwitch", switch.Switch)
SwitchType = ld6002b_ns.enum("SwitchType", is_class=True)

# None of these three carry an inversion. They name what the module is doing, not
# how something is wired to it, so an inverted one would only report the opposite
# of the truth -- and the boot restore, which applies a state nothing reports back,
# is where that would be hardest to spot.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_LOW_POWER): switch.switch_schema(
            LD6002BSwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_POINT_CLOUD): switch.switch_schema(
            LD6002BSwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TARGET_DISPLAY): switch.switch_schema(
            LD6002BSwitch,
            block_inverted=True,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_ON",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    for key, switch_type, setter in (
        (CONF_LOW_POWER, SwitchType.LOW_POWER, "set_low_power_switch"),
        (CONF_POINT_CLOUD, SwitchType.POINT_CLOUD, "set_point_cloud_switch"),
        (CONF_TARGET_DISPLAY, SwitchType.TARGET_DISPLAY, "set_target_display_switch"),
    ):
        if conf := config.get(key):
            s = await switch.new_switch(conf, switch_type)
            await cg.register_parented(s, config[CONF_LD6002B_ID])
            cg.add(getattr(hub, setter)(s))
