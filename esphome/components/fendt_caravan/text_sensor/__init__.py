import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_PARENT_ID, FendtCaravanHubBase

CONF_SOFTWARE_VERSION = "software_version"
CONF_FRIDGE_POWER_SOURCE = "fridge_power_source"
CONF_FRIDGE_TYPE = "fridge_type"


def _text_schema(
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    return text_sensor.text_sensor_schema(
        device_class=device_class,
        entity_category=entity_category,
        icon=icon,
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SOFTWARE_VERSION: _text_schema(icon="mdi:application-braces-outline"),
        CONF_FRIDGE_POWER_SOURCE: _text_schema(icon="mdi:power-plug-battery"),
        CONF_FRIDGE_TYPE: _text_schema(icon="mdi:fridge-top"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_text_sensor")(var))
