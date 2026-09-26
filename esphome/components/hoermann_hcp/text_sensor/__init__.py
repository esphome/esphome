import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_VERSION, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from .. import CONF_HOERMANN_HCP_ID, HoermannHcp

DEPENDENCIES = ["hoermann_hcp"]

CONF_SERIAL_NUMBER = "serial_number"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HOERMANN_HCP_ID): cv.use_id(HoermannHcp),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
                icon="mdi:data-matrix", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
                icon=ICON_CHIP, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_SERIAL_NUMBER, CONF_VERSION),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_HOERMANN_HCP_ID])
    cg.add_define("USE_HOERMANN_HCP_TEXT_SENSOR")
    if (conf := config.get(CONF_SERIAL_NUMBER)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_serial_number_text_sensor(sens))
    if (conf := config.get(CONF_VERSION)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.set_version_text_sensor(sens))
