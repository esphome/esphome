from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import (
    CONF_ID,
    CONF_ICON,
    CONF_ENTITY_CATEGORY,
    ENTITY_CATEGORY_CONFIG,
)

from .. import dxs238xw_ns, CONF_DXS238XW_ID, SmIdEntity, DXS238XW_COMPONENT_SCHEMA

DEPENDENCIES = ["dxs238xw"]

Dxs238xwSwitch = dxs238xw_ns.class_("Dxs238xwSwitch", switch.Switch)

ENERGY_PURCHASE_STATE = "energy_purchase_state"
METER_STATE = "meter_state"
DELAY_STATE = "delay_state"

TYPES = {
    ENERGY_PURCHASE_STATE: (
        switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(Dxs238xwSwitch),
                cv.Optional(CONF_ICON, default="mdi:lightning-bolt"): cv.icon,
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
                ): cv.entity_category,
            }
        ),
        SmIdEntity.SWITCH_ENERGY_PURCHASE_STATE,
    ),
    METER_STATE: (
        switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(Dxs238xwSwitch),
                cv.Optional(CONF_ICON, default="mdi:power-plug"): cv.icon,
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
                ): cv.entity_category,
            }
        ),
        SmIdEntity.SWITCH_METER_STATE,
    ),
    DELAY_STATE: (
        switch.SWITCH_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(Dxs238xwSwitch),
                cv.Optional(CONF_ICON, default="mdi:timer-cog-outline"): cv.icon,
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
                ): cv.entity_category,
            }
        ),
        SmIdEntity.SWITCH_DELAY_STATE,
    ),
}

CONFIG_SCHEMA = DXS238XW_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, (schema, _) in TYPES.items()}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type, (_, number_id) in TYPES.items():
        if type in config:
            conf = config[type]
            var = await switch.new_switch(conf)
            cg.add(var.set_dxs238xw_parent(paren))
            cg.add(var.set_entity_id(number_id))
            cg.add(getattr(paren, f"set_{type}_switch")(var))
