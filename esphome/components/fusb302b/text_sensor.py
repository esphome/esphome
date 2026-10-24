import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_FLASH
from esphome.types import ConfigType

from . import CONF_FUSB302B_ID, FUSB302B

DEPENDENCIES = ["fusb302b"]

CONF_CONTRACT = "contract"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FUSB302B_ID): cv.use_id(FUSB302B),
        cv.Optional(CONF_CONTRACT): text_sensor.text_sensor_schema(
            icon=ICON_FLASH,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_FUSB302B_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_CONTRACT, hub.set_contract_text_sensor)
