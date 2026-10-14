import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ICON_LIGHTBULB,
    ICON_RULER,
    ICON_SCALE,
    ICON_THERMOMETER,
)
from esphome.types import ConfigType

from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

BaudRateSelect = ld2410_ns.class_("BaudRateSelect", select.Select)
DistanceResolutionSelect = ld2410_ns.class_("DistanceResolutionSelect", select.Select)
LightOutControlSelect = ld2410_ns.class_("LightOutControlSelect", select.Select)

CONF_DISTANCE_RESOLUTION = "distance_resolution"
CONF_LIGHT_FUNCTION = "light_function"
CONF_OUT_PIN_LEVEL = "out_pin_level"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_DISTANCE_RESOLUTION): select.select_schema(
        DistanceResolutionSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RULER,
    ),
    cv.Optional(CONF_LIGHT_FUNCTION): select.select_schema(
        LightOutControlSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_LIGHTBULB,
    ),
    cv.Optional(CONF_OUT_PIN_LEVEL): select.select_schema(
        LightOutControlSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        BaudRateSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_THERMOMETER,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_DISTANCE_RESOLUTION,
        hub.set_distance_resolution_select,
        options=["0.2m", "0.75m"],
    )
    await selects(
        CONF_OUT_PIN_LEVEL, hub.set_out_pin_level_select, options=["low", "high"]
    )
    await selects(
        CONF_LIGHT_FUNCTION,
        hub.set_light_function_select,
        options=["off", "below", "above"],
    )
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
