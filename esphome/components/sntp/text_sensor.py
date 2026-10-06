import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
import esphome.final_validate as fv
from esphome.types import ConfigType

from .time import CONF_SNTP_ID, SNTPComponent, require_timezone_service

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_SNTP_ID): cv.use_id(SNTPComponent),
    }
)


def _require_timezone_service(sntp_config: ConfigType) -> ConfigType:
    return require_timezone_service(sntp_config, "The sntp text sensor")


def _final_validate(config: ConfigType) -> None:
    fv.id_declaration_match_schema(_require_timezone_service)(config[CONF_SNTP_ID])


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_SNTP_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(parent.set_timezone_abbreviation_text_sensor(var))
