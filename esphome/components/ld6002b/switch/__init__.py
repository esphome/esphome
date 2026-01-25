import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_LOW_POWER): switch.switch_schema(
            LD6002BSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_POINT_CLOUD): switch.switch_schema(
            LD6002BSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TARGET_DISPLAY): switch.switch_schema(
            LD6002BSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    if low_power_config := config.get(CONF_LOW_POWER):
        s = cg.new_Pvariable(low_power_config[CONF_ID], SwitchType.LOW_POWER)
        await switch.register_switch(s, low_power_config)
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_low_power_switch(s))

    if point_cloud_config := config.get(CONF_POINT_CLOUD):
        s = cg.new_Pvariable(point_cloud_config[CONF_ID], SwitchType.POINT_CLOUD)
        await switch.register_switch(s, point_cloud_config)
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_point_cloud_switch(s))

    if target_display_config := config.get(CONF_TARGET_DISPLAY):
        s = cg.new_Pvariable(target_display_config[CONF_ID], SwitchType.TARGET_DISPLAY)
        await switch.register_switch(s, target_display_config)
        await cg.register_parented(s, config[CONF_LD6002B_ID])
        cg.add(hub.set_target_display_switch(s))
