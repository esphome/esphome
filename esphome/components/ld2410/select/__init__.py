import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_BAUD_RATE
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

LD2410Select = ld2410_ns.class_("LD2410Select", select.Select)

CONF_DISTANCE_RESOLUTION = "distance_resolution"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_DISTANCE_RESOLUTION): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:social-distance-2-meters",
    ),
    cv.Optional(CONF_BAUD_RATE): select.select_schema(
        LD2410Select,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:speedometer",
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_DISTANCE_RESOLUTION in config:
        s = await select.new_select(
            config[CONF_DISTANCE_RESOLUTION], options=["0.2m", "0.75m"]
        )
        cg.add(ld2410_component.set_distance_resolution_select(s))
    if CONF_BAUD_RATE in config:
        s = await select.new_select(
            config[CONF_BAUD_RATE],
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
        cg.add(ld2410_component.set_baud_rate_select(s))
