import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_SECURITY,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_RDS,
    CONF_MUTE,
    CONF_MONO,
    CONF_TX_ENABLE,
    CONF_PRIV_EN,
    CONF_ENABLE,
    ICON_VOLUME_MUTE,
    ICON_EAR_HEARING,
    ICON_RADIO_TOWER,
    ICON_FORMAT_TEXT,
    for_each_conf,
)

MuteSwitch = qn8027_ns.class_("MuteSwitch", switch.Switch)
MonoSwitch = qn8027_ns.class_("MonoSwitch", switch.Switch)
TxEnableSwitch = qn8027_ns.class_("TxEnableSwitch", switch.Switch)
PrivEnSwitch = qn8027_ns.class_("PrivEnSwitch", switch.Switch)
RDSEnableSwitch = qn8027_ns.class_("RDSEnableSwitch", switch.Switch)

RDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE): switch.switch_schema(
            RDSEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_FORMAT_TEXT,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_QN8027_ID): cv.use_id(QN8027Component),
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
        cv.Optional(CONF_TX_ENABLE): switch.switch_schema(
            TxEnableSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RADIO_TOWER,
        ),
        cv.Optional(CONF_PRIV_EN): switch.switch_schema(
            PrivEnSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SECURITY,
        ),
        cv.Optional(CONF_RDS): RDS_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [CONF_MUTE],
        [CONF_MONO],
        [CONF_TX_ENABLE],
        [CONF_PRIV_EN],
    ],
    CONF_RDS: [
        [CONF_ENABLE],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_QN8027_ID])

    async def new_switch(c, args, setter):
        s = await switch.new_switch(c)
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_switch")(s))

    await for_each_conf(config, VARIABLES, new_switch)
