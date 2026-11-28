import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import ATSHA204A, CONF_ATSHA204A_ID, DOMAIN

DEPENDENCIES = [DOMAIN]

CONF_CHIP_REVISION = "chip_revision"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ATSHA204A_ID): cv.use_id(ATSHA204A),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_CHIP_REVISION): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_SERIAL_NUMBER, CONF_CHIP_REVISION),
)


FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=text_sensor.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_ATSHA204A_ID,),
    message="Only one ATSHA204A text sensor platform entry is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ATSHA204A_ID])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(CONF_SERIAL_NUMBER, hub.set_serial_number_text_sensor)
    await sensors(CONF_CHIP_REVISION, hub.set_chip_revision_text_sensor)
