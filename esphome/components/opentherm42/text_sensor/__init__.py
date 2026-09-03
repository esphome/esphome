import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import OpenTherm42Hub
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BRAND,
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER,
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION,
    CONF_OPENTHERM42_ID,
)

TYPES: dict[str, cv.Schema] = {
    # §5.3.2 Class 2, ID 93: Brand -- the boiler manufacturer's brand name, read one ASCII character
    # at a time (index in HB, character in LB) until the boiler-reported character count is reached.
    CONF_CONFIGURATION_INFORMATION_BRAND: text_sensor.text_sensor_schema(),
    # §5.3.2 Class 2, ID 94: Brand version, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_VERSION: text_sensor.text_sensor_schema(),
    # §5.3.2 Class 2, ID 95: Brand serial number, read the same way as ID 93.
    CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER: text_sensor.text_sensor_schema(),
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
