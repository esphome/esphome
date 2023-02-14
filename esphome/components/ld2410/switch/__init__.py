import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

VirtualSwitch = ld2410_ns.class_("VirtualSwitch", switch.Switch)

CONF_ENGINEERING_MODE = "engineering_mode"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_ENGINEERING_MODE): switch.switch_schema(
        VirtualSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        icon="mdi:wrench",
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_ENGINEERING_MODE in config:
        s = await switch.new_switch(config[CONF_ENGINEERING_MODE])
        cg.add(ld2410_component.set_engineering_mode_switch(s))
