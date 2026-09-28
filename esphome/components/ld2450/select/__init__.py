import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ICON_THERMOMETER,
)
from esphome.types import ConfigType

from .. import CONF_LD2450_ID, LD2450Component, ld2450_ns

CONF_ZONE_TYPE = "zone_type"

BaudRateSelect = ld2450_ns.class_("BaudRateSelect", select.Select)
ZoneTypeSelect = ld2450_ns.class_("ZoneTypeSelect", select.Select)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        BaudRateSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_THERMOMETER,
    ),
    cv.Optional(CONF_ZONE_TYPE): select.select_schema(
        ZoneTypeSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_THERMOMETER,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_BAUD_RATE,
        hub.set_baud_rate_select,
        options=[
            "9600",
            "19200",
            "38400",
            "57600",
            "115200",
            "230400",
            "256000",
            "460800",
        ],
    )
    await selects(
        CONF_ZONE_TYPE,
        hub.set_zone_type_select,
        options=[
            "Disabled",
            "Detection",
            "Filter",
        ],
    )
