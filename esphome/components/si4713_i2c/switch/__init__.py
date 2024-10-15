import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_SECURITY,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_MUTE,
    CONF_MONO,
    CONF_RDS_ENABLE,
    ICON_VOLUME_MUTE,
    ICON_EAR_HEARING,
    ICON_FORMAT_TEXT,
)

CONF_GPIO1="gpio1"
CONF_GPIO2="gpio2"
CONF_GPIO3="gpio3"

MuteSwitch = si4713_ns.class_("MuteSwitch", switch.Switch)
MonoSwitch = si4713_ns.class_("MonoSwitch", switch.Switch)
RDSEnableSwitch = si4713_ns.class_("RDSEnableSwitch", switch.Switch)
GPIOSwitch = si4713_ns.class_("GPIOSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
        cv.Optional(CONF_MUTE): switch.switch_schema(
            MuteSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_VOLUME_MUTE,
        ),
        cv.Optional(CONF_MONO): switch.switch_schema(
            MonoSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_EAR_HEARING,
        ),
        cv.Optional(CONF_RDS_ENABLE): switch.switch_schema(
            RDSEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FORMAT_TEXT,
        ),
        cv.Optional(CONF_GPIO1): switch.switch_schema(
            GPIOSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
        cv.Optional(CONF_GPIO2): switch.switch_schema(
            GPIOSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
        cv.Optional(CONF_GPIO3): switch.switch_schema(
            GPIOSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=,
        ),
    }
)


async def new_switch(config, id, setter):
    if c := config.get(id):
        s = await switch.new_switch(c)
        await cg.register_parented(s, config[CONF_SI4713_ID])
        cg.add(setter(s))
        return s


async def to_code(config):
    c = await cg.get_variable(config[CONF_SI4713_ID])
    await new_switch(config, CONF_MUTE, c.set_mute_switch)
    await new_switch(config, CONF_MONO, c.set_mono_switch)
    await new_switch(config, CONF_RDS_ENABLE, c.set_rds_enable_switch)
    s = await new_switch(config, CONF_GPIO1, c.set_gpio1_switch)
    s.set_pin(1)
    s = await new_switch(config, CONF_GPIO2, c.set_gpio2_switch)
    s.set_pin(2)
    s = await new_switch(config, CONF_GPIO3, c.set_gpio3_switch)
    s.set_pin(3)
