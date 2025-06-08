from typing import Any

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_VALUE, CONF_RESTORE_VALUE, CONF_STEP

from .. import const, generate, input, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.NUMBER

OpenthermNumber = generate.opentherm_ns.class_(
    "OpenthermNumber", number.Number, cg.Component, input.OpenthermInput
)


async def new_openthermnumber(config: dict[str, Any]) -> cg.Pvariable:
    var = await number.new_number(
        config,
        min_value=config[input.CONF_min_value],
        max_value=config[input.CONF_max_value],
        step=config[input.CONF_step],
    )
    await cg.register_component(var, config)
    input.generate_setters(var, config)

    if (initial_value := config.get(CONF_INITIAL_VALUE, None)) is not None:
        cg.add(var.set_initial_value(initial_value))
    if (restore_value := config.get(CONF_RESTORE_VALUE, None)) is not None:
        cg.add(var.set_restore_value(restore_value))

    return var


def get_entity_validation_schema(entity: schema.InputSchema) -> cv.Schema:
    return (
        number.number_schema(
            OpenthermNumber, unit_of_measurement=entity.unit_of_measurement
        )
        .extend(
            {
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
