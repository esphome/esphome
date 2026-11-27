import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MAX_CURRENT, ENTITY_CATEGORY_CONFIG, UNIT_AMPERE
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox

DEPENDENCIES = ["delta_wallbox"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_MAX_CURRENT): number.number_schema(
            number.ApplyNumber,
            unit_of_measurement=UNIT_AMPERE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:flash",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])
    numbers = number.sub_numbers(config, parent=config[CONF_DELTA_WALLBOX_ID])
    await numbers(
        CONF_MAX_CURRENT,
        hub.set_max_current_number,
        min_value=6,
        max_value=32,
        step=1,
        apply="set_max_current(value)",
    )
