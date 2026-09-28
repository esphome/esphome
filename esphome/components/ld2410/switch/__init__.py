import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUETOOTH,
    CONF_ID,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_BLUETOOTH,
    ICON_PULSE,
)
from esphome.types import ConfigType

from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

BluetoothSwitch = ld2410_ns.class_("BluetoothSwitch", switch.Switch)
EngineeringModeSwitch = ld2410_ns.class_("EngineeringModeSwitch", switch.Switch)

CONF_ENGINEERING_MODE = "engineering_mode"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_ENGINEERING_MODE): switch.switch_schema(
        EngineeringModeSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_PULSE,
    ),
    cv.Optional(CONF_BLUETOOTH): switch.switch_schema(
        BluetoothSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_BLUETOOTH,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_ENGINEERING_MODE, hub.set_engineering_mode_switch)
    await switches(CONF_BLUETOOTH, hub.set_bluetooth_switch)
