import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUETOOTH,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_BLUETOOTH,
    ICON_PULSE,
)

from .. import CONF_LD2412_ID, LD2412_ns, LD2412Component

BluetoothSwitch = LD2412_ns.class_("BluetoothSwitch", switch.Switch)
EngineeringModeSwitch = LD2412_ns.class_("EngineeringModeSwitch", switch.Switch)

CONF_ENGINEERING_MODE = "engineering_mode"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
    cv.Optional(CONF_BLUETOOTH): switch.switch_schema(
        BluetoothSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_BLUETOOTH,
    ),
    cv.Optional(CONF_ENGINEERING_MODE): switch.switch_schema(
        EngineeringModeSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
}


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if bluetooth_config := config.get(CONF_BLUETOOTH):
        s = await switch.new_switch(bluetooth_config)
        await cg.register_parented(s, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_bluetooth_switch(s))
    if engineering_mode_config := config.get(CONF_ENGINEERING_MODE):
        s = await switch.new_switch(engineering_mode_config)
        await cg.register_parented(s, config[CONF_LD2412_ID])
        cg.add(LD2412_component.set_engineering_mode_switch(s))
