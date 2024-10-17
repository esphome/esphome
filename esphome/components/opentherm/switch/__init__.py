from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from .. import const, schema, validate, generate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.SWITCH

OpenthermSwitch = generate.opentherm_ns.class_(
    "OpenthermSwitch", switch.Switch, cg.Component
)


async def new_openthermswitch(config: dict[str, Any]) -> cg.Pvariable:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    return var


def get_entity_validation_schema(entity: schema.SwitchSchema) -> cv.Schema:
    return switch.SWITCH_SCHEMA.extend(
        {cv.GenerateID(): cv.declare_id(OpenthermSwitch)}
    ).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = validate.create_component_schema(
    schema.SWITCHES, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    keys = await generate.component_to_code(
        COMPONENT_TYPE,
        schema.SWITCHES,
        OpenthermSwitch,
        generate.create_only_conf(new_openthermswitch),
        config,
    )
    generate.define_readers(COMPONENT_TYPE, keys)
