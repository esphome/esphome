import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM, CONF_TIME, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from .time import CONF_SNTP, CONF_SNTP_ID, ZONE_IP, SNTPComponent, uses_timezone_service

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_SNTP_ID): cv.use_id(SNTPComponent),
    }
)


async def to_code(config: ConfigType) -> None:
    sntp_conf = next(
        (
            conf
            for conf in CORE.config.get(CONF_TIME, [])
            if conf.get(CONF_PLATFORM) == CONF_SNTP
            and conf[CONF_ID].id == config[CONF_SNTP_ID].id
        ),
        None,
    )
    if sntp_conf is None or not uses_timezone_service(sntp_conf):
        raise EsphomeError(
            "The sntp timezone abbreviation text sensor needs the sntp time "
            f"'timezone' option to be set to a service, for example 'timezone: {{zone: {ZONE_IP}}}'"
        )
    parent = await cg.get_variable(config[CONF_SNTP_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(parent.set_timezone_abbreviation_text_sensor(var))
