import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_LD2410S_ID, LD2410SComponent

DEPENDENCIES = ["ld2410s"]

CONF_RESPONSE_SPEED = "response_speed"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410SComponent),
    cv.Required(CONF_RESPONSE_SPEED): select.select_schema(
        select.ApplySelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:speedometer",
    ),
}


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD2410S_PARAMETERS")
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_RESPONSE_SPEED,
        hub.set_response_speed_select,
        options=["Normal", "Fast"],
        apply="control_response_speed(index)",
    )
