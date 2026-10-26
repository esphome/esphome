import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_OUT_PIN_LEVEL
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

from .. import CONF_LD2410_ID, LD2410Component

CONF_DISTANCE_RESOLUTION = "distance_resolution"
CONF_LIGHT_FUNCTION = "light_function"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_DISTANCE_RESOLUTION): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RULER,
    ),
    cv.Optional(CONF_LIGHT_FUNCTION): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_LIGHTBULB,
    ),
    cv.Optional(CONF_OUT_PIN_LEVEL): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SCALE,
    ),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_THERMOMETER,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    selects = select.sub_selects(config, parent=hub)
    # Option lists follow the order of the C++ enums and tables; the hub maps by index
    await selects(
        CONF_DISTANCE_RESOLUTION,
        hub.set_distance_resolution_select,
        options=["0.2m", "0.75m"],
        apply=(
            "set_distance_resolution(static_cast<ld2410::DistanceResolution>("
            "progmem_read_byte(&ld2410::DISTANCE_RESOLUTIONS[index])))"
        ),
        publish="before",
    )
    light = {"apply": "set_light_out_control()", "publish": "before"}
    await selects(
        CONF_OUT_PIN_LEVEL,
        hub.set_out_pin_level_select,
        options=["low", "high"],
        **light,
    )
    await selects(
        CONF_LIGHT_FUNCTION,
        hub.set_light_function_select,
        options=["off", "below", "above"],
        **light,
    )
    await selects(
        CONF_BAUD_RATE,
        hub.set_baud_rate_select,
        apply="set_baud_rate(static_cast<ld2410::BaudRate>(index + 1))",
        publish="before",
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
