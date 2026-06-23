from typing import Any

import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import const, generate, input, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.OUTPUT

OpenthermOutput = generate.opentherm_ns.class_(
    "OpenthermOutput",
    output.FloatOutput,
    cg.Component,
    input.OpenthermInput,
    generate.MessageProcessor,
)


async def new_openthermoutput(
    config: dict[str, Any], key: str, hub: cg.MockObj
) -> cg.MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    input.generate_setters(var, config)
    return var


def get_entity_validation_schema(entity: schema.InputSchema) -> cv.Schema:
    return (
        output.FLOAT_OUTPUT_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(OpenthermOutput)}
        )
        .extend(input.input_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.INPUTS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    await generate.component_to_code(
        COMPONENT_TYPE,
        schema.INPUTS,
        OpenthermOutput,
        new_openthermoutput,
        config,
    )
