from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_STEP,
    CONF_INITIAL_VALUE,
    CONF_RESTORE_VALUE,
)
from .. import const, schema, validate, input, generate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.NUMBER

OpenthermNumber = generate.opentherm_ns.class_(
    "OpenthermNumber", number.Number, cg.Component, input.OpenthermInput
)


async def new_openthermnumber(config: dict[str, Any]) -> cg.Pvariable:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[input.CONF_min_value],
        max_value=config[input.CONF_max_value],
        step=config[input.CONF_step],
    )
    input.generate_setters(var, config)

    if CONF_INITIAL_VALUE in config:
        cg.add(var.set_initial_value(config[CONF_INITIAL_VALUE]))
    if CONF_RESTORE_VALUE in config:
        cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))

    return var


def get_entity_validation_schema(entity: schema.InputSchema) -> cv.Schema:
    return (
        number.NUMBER_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(OpenthermNumber),
                cv.Optional(
                    CONF_UNIT_OF_MEASUREMENT, entity.unit_of_measurement
                ): cv.string_strict,
                cv.Optional(CONF_STEP, entity.step): cv.float_,
                cv.Optional(CONF_INITIAL_VALUE): cv.float_,
                cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
            }
        )
        .extend(input.input_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.INPUTS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    keys = await generate.component_to_code(
        COMPONENT_TYPE,
        schema.INPUTS,
        OpenthermNumber,
        generate.create_only_conf(new_openthermnumber),
        config,
    )
    generate.define_readers(COMPONENT_TYPE, keys)
