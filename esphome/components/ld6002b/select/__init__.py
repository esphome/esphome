from functools import partial

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_AREA_ID, CONF_SENSITIVITY, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import DispatchEntity, LD6002BComponent, build_type_dispatch, ld6002b_ns
from ..const import CONF_INSTALLATION_MODE, CONF_LD6002B_ID, CONF_TRIGGER_SPEED

DEPENDENCIES = ["ld6002b"]

SelectType = ld6002b_ns.enum("SelectType", is_class=True)

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


SELECT_MAP = (
    (
        CONF_SENSITIVITY,
        SelectType.SENSITIVITY,
        "set_sensitivity_select",
        ["low", "medium", "high"],
    ),
    (
        CONF_TRIGGER_SPEED,
        SelectType.TRIGGER_SPEED,
        "set_trigger_speed_select",
        ["slow", "medium", "fast"],
    ),
    (
        CONF_INSTALLATION_MODE,
        SelectType.INSTALLATION_MODE,
        "set_installation_select",
        ["top", "side"],
    ),
    (CONF_AREA_ID, SelectType.AREA_ID, "set_area_id_select", AREA_ID_OPTIONS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    entities: list[DispatchEntity] = []
    for key, select_type, setter, options in SELECT_MAP:
        if (conf := config.get(key)) is not None:
            create = partial(select.new_select, conf, options=options)
            entities.append((select_type, conf, create, getattr(hub, setter)))
    await build_type_dispatch(
        hub,
        "select::Select *entity, size_t index",
        "set_select_value(type, index)",
        entities,
        publish_before="entity->publish_state(index);",
    )
