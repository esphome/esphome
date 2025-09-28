from collections.abc import Callable

from voluptuous import Schema

import esphome.config_validation as cv
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import const, generate
from .schema import TSchema


def create_entities_schema(
    entities: dict[str, TSchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    entity_schema = {}
    for key, entity in entities.items():
        entity_schema[cv.Optional(key)] = get_entity_validation_schema(entity)
    return cv.Schema(entity_schema)


def create_component_schema(
    entities: dict[str, TSchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    return (
        cv.Schema(
            {cv.GenerateID(const.CONF_OPENTHERM_ID): cv.use_id(generate.OpenthermHub)}
        )
        .extend(create_entities_schema(entities, get_entity_validation_schema))
        .extend(cv.COMPONENT_SCHEMA)
    )


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    ot_config = full_config.get("opentherm", [])

    if len(ot_config) > 1 and not CORE.is_esp32:
        raise cv.Invalid("Multiple OpenTherm instances are only possible on ESP32")
