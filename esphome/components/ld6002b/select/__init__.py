import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_AREA_ID, CONF_SENSITIVITY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import LD6002BComponent, validate_unique_settings
from ..const import CONF_INSTALLATION_MODE, CONF_LD6002B_ID, CONF_TRIGGER_SPEED

DEPENDENCIES = ["ld6002b"]

AREA_ID_OPTIONS = [
    "interference_area_0",
    "interference_area_1",
    "interference_area_2",
    "interference_area_3",
    "detection_area_0",
    "detection_area_1",
    "detection_area_2",
    "detection_area_3",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_SENSITIVITY): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_TRIGGER_SPEED): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_INSTALLATION_MODE): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_AREA_ID): select.select_schema(
            select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
    }
)


def final_validate(config: ConfigType) -> None:
    validate_unique_settings(select.DOMAIN, config)


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    control = {"apply": "set_select_value(entity, index)", "publish": "before"}
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_SENSITIVITY,
        hub.set_sensitivity_select,
        options=["low", "medium", "high"],
        **control,
    )
    await selects(
        CONF_TRIGGER_SPEED,
        hub.set_trigger_speed_select,
        options=["slow", "medium", "fast"],
        **control,
    )
    await selects(
        CONF_INSTALLATION_MODE,
        hub.set_installation_select,
        options=["top", "side"],
        **control,
    )
    await selects(
        CONF_AREA_ID, hub.set_area_id_select, options=AREA_ID_OPTIONS, **control
    )
