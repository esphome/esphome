import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from . import CONF_LD2460_ID, LD2460Component

DEPENDENCIES = ["ld2460"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])
    if version_config := config.get(CONF_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(ld2460_component.set_version_text_sensor(sens))
