from typing import Callable

import esphome.config_validation as cv

from . import const, schema, generate

def create_entities_schema(entities: schema.Schema[schema.T], get_entity_validation_schema: Callable[[schema.T], cv.Schema]) -> cv.Schema:
    schema = {}
    for key, entity in entities.items():
        schema[cv.Optional(key)] = get_entity_validation_schema(entity)
    return cv.Schema(schema)

def create_component_schema(entities: schema.Schema[schema.T], get_entity_validation_schema: Callable[[schema.T], cv.Schema]) -> cv.Schema:
    return cv.Schema({ cv.GenerateID(const.CONF_OPENTHERM_ID): cv.use_id(generate.OpenthermHub) }) \
        .extend(create_entities_schema(entities, get_entity_validation_schema)) \
        .extend(cv.COMPONENT_SCHEMA)
