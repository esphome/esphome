import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_PARENT_ID, FendtCaravanHubBase


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
        "software_version": _text_schema(icon="mdi:application-braces-outline"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_text_sensor")(var))
