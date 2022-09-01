from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_DXS238XW_ID, DXS238XW_COMPONENT_SCHEMA

DEPENDENCIES = ["dxs238xw"]

CONF_METER_STATE_DETAIL = "meter_state_detail"
CONF_DELAY_VALUE_REMAINING = "delay_value_remaining"
CONF_METER_ID = "meter_id"

TYPES = {
    CONF_METER_STATE_DETAIL: text_sensor.text_sensor_schema(
        icon="mdi:power-plug",
    ),
    CONF_DELAY_VALUE_REMAINING: text_sensor.text_sensor_schema(
        icon="mdi:timer-cog-outline",
    ),
    CONF_METER_ID: text_sensor.text_sensor_schema(
        icon="mdi:alpha-s-box-outline",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = DXS238XW_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in TYPES.items()}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_DXS238XW_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            var = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(paren, f"set_{type}_text_sensor")(var))
