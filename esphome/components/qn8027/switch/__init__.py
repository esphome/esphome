import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_MUTE,
    CONF_MONO,
    CONF_TX_ENABLE,
    ICON_VOLUME_MUTE,
    ICON_EAR_HEARING,
    ICON_RADIO_TOWER,
)

MuteSwitch = qn8027_ns.class_("MuteSwitch", switch.Switch)
MonoSwitch = qn8027_ns.class_("MonoSwitch", switch.Switch)
TxEnableSwitch = qn8027_ns.class_("TxEnableSwitch", switch.Switch)

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
    }
)

async def to_code(config):
    qn8027_component = await cg.get_variable(config[CONF_QN8027_ID])
    if mute_config := config.get(CONF_MUTE):
        s = await switch.new_switch(mute_config)
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_mute_switch(s))
    if mono_config := config.get(CONF_MONO):
        s = await switch.new_switch(mono_config)
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_mono_switch(s))
    if tx_enable_config := config.get(CONF_TX_ENABLE):
        s = await switch.new_switch(tx_enable_config)
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_tx_enable_switch(s))
