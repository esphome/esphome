from esphome import final_validate as fv
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_PARENT_ID, DOMAIN, MainControlUnitHub

CONF_SOFTWARE_VERSION = "software_version"
CONF_PANEL_VERSION = "panel_version"


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
            cv.Required(CONF_PARENT_ID): cv.use_id(MainControlUnitHub),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SOFTWARE_VERSION: _text_schema(
            MainControlUnitHub, icon="mdi:application-braces-outline"
        ),
        CONF_PANEL_VERSION: _text_schema(MainControlUnitHub, icon="mdi:monitor"),
    }
)


# The hub keeps one pointer per type, so a second entry would take over the first one's updates
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="text_sensor",
    platform=DOMAIN,
    keys=(CONF_PARENT_ID, CONF_TYPE),
    message="Only one fendt_caravan text sensor of type '{type}' is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PARENT_ID])
    setter = {
        CONF_SOFTWARE_VERSION: hub.set_software_version_text_sensor,
        CONF_PANEL_VERSION: hub.set_panel_version_text_sensor,
    }[config[CONF_TYPE]]
    var = await text_sensor.new_text_sensor(config)
    cg.add(setter(var))
