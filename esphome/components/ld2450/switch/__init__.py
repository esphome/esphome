import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_BLUETOOTH,
    ICON_PULSE,
)

from .. import CONF_LD2450_ID, LD2450Component, ld2450_ns

BluetoothSwitch = ld2450_ns.class_("BluetoothSwitch", switch.Switch)
MultiTargetSwitch = ld2450_ns.class_("MultiTargetSwitch", switch.Switch)

CONF_BLUETOOTH = "bluetooth"
CONF_MULTI_TARGET = "multi_target"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_BLUETOOTH): switch.switch_schema(
        BluetoothSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_BLUETOOTH,
    ),
    cv.Optional(CONF_MULTI_TARGET): switch.switch_schema(
        MultiTargetSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
}


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if bluetooth_config := config.get(CONF_BLUETOOTH):
        s = await switch.new_switch(bluetooth_config)
        await cg.register_parented(s, config[CONF_LD2450_ID])
        cg.add(ld2450_component.set_bluetooth_switch(s))
    if multi_target_config := config.get(CONF_MULTI_TARGET):
        s = await switch.new_switch(multi_target_config)
        await cg.register_parented(s, config[CONF_LD2450_ID])
        cg.add(ld2450_component.set_multi_target_switch(s))
