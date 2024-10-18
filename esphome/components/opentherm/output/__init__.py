from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID
from .. import const, schema, validate, input, generate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.OUTPUT

OpenthermOutput = generate.opentherm_ns.class_(
    "OpenthermOutput", output.FloatOutput, cg.Component, input.OpenthermInput
)


async def new_openthermoutput(
    config: dict[str, Any], key: str, _hub: cg.MockObj
) -> cg.Pvariable:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(getattr(var, "set_id")(cg.RawExpression(f'"{key}_{config[CONF_ID]}"')))
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
    keys = await generate.component_to_code(
        COMPONENT_TYPE, schema.INPUTS, OpenthermOutput, new_openthermoutput, config
    )
    generate.define_readers(COMPONENT_TYPE, keys)
