from typing import Callable

from voluptuous import Schema

import esphome.config_validation as cv

from . import const, schema, generate
from .schema import TSchema


def create_entities_schema(
    entities: dict[str, TSchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    entity_schema = {}
    for key, entity in entities.items():
        schema_key = (
            cv.Optional(key, entity.default_value)
            if hasattr(entity, "default_value")
            else cv.Optional(key)
        )
        entity_schema[schema_key] = get_entity_validation_schema(entity)
    return cv.Schema(entity_schema)


def create_component_schema(
    entities: dict[str, schema.EntitySchema],
    get_entity_validation_schema: Callable[[TSchema], cv.Schema],
) -> Schema:
    return (
        cv.Schema(
            {cv.GenerateID(const.CONF_OPENTHERM_ID): cv.use_id(generate.OpenthermHub)}
        )
        .extend(create_entities_schema(entities, get_entity_validation_schema))
        .extend(cv.COMPONENT_SCHEMA)
    )
