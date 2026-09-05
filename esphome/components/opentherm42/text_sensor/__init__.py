import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BRAND,
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER,
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION,
    CONF_OPENTHERM42_ID,
)

# device_class is left unset for all three: Home Assistant text sensors only support "date" and
# "timestamp", and a brand name/version/serial number is neither. entity_category is DIAGNOSTIC:
# these are static Class 2 ("Configuration Information") identification strings the boiler reports
# once, the same nature as HA's own example of a diagnostic entity ("a sensor showing... MAC
# address"), not something that changes or that a user watches day-to-day.
TYPES: dict[str, cv.Schema] = {
    # §5.3.2 Class 2, ID 93: Brand -- the boiler manufacturer's brand name, read one ASCII character
    # at a time (index in HB, character in LB) until the boiler-reported character count is reached.
    CONF_CONFIGURATION_INFORMATION_BRAND: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 94: Brand version, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 95: Brand serial number, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER: text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await text_sensor.new_text_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_text_sensor")(var))
