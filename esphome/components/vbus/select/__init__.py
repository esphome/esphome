"""Confirmed enumerated controls for DeltaSol CS Plus 1.11."""

from functools import partial

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_OPTIONS, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import (
    CONF_CONTROLLER_ID,
    CONF_PARAMETER,
    ResolDeltaSolCSPlus,
    register_setting_entity,
    setting_configs,
    setting_entity_defaults,
    setting_group_defaults,
    setting_platform_schema,
    validate_setting_controller,
    vbus_ns,
)
from ..cs_plus_selects import SELECT_PARAMETERS

DEPENDENCIES = ["vbus"]
VBusSettingSelect = vbus_ns.class_("VBusSettingSelect", select.Select)


def validate_options(config: ConfigType) -> ConfigType:
    spec = SELECT_PARAMETERS[config[CONF_PARAMETER]]
    defaults = dict(enumerate(spec.labels, start=spec.minimum))
    options = config.get(CONF_OPTIONS, defaults)
    if set(options) != set(defaults):
        raise cv.Invalid("Option labels must cover exactly the documented raw values")
    if len(set(options.values())) != len(options):
        raise cv.Invalid("Option labels must be unique")
    if any("\0" in label for label in options.values()):
        raise cv.Invalid("Option labels must not contain null characters")
    config[CONF_OPTIONS] = options
    return config


ENTITY_SCHEMA = select.select_schema(
    VBusSettingSelect, entity_category=ENTITY_CATEGORY_CONFIG
).extend(
    {
        cv.Optional(CONF_OPTIONS): cv.Schema(
            {cv.int_: cv.All(cv.string_strict, cv.Length(min=1))}
        ),
    }
)
SINGLE_SCHEMA = cv.All(
    setting_entity_defaults,
    ENTITY_SCHEMA.extend(
        {
            cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            cv.Required(CONF_PARAMETER): cv.one_of(*SELECT_PARAMETERS, lower=True),
        }
    ),
    validate_options,
)


def validate_group_options(config: ConfigType) -> ConfigType:
    for entity in setting_configs(config, SELECT_PARAMETERS):
        validate_options(entity)
        config[entity[CONF_PARAMETER]][CONF_OPTIONS] = entity[CONF_OPTIONS]
    return config


GROUP_SCHEMA = cv.All(
    partial(
        setting_group_defaults,
        parameters=SELECT_PARAMETERS,
        defaults=setting_entity_defaults,
    ),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(cg.EntityBase),
            cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            **{cv.Optional(key): ENTITY_SCHEMA for key in SELECT_PARAMETERS},
        }
    ),
    cv.has_at_least_one_key(*SELECT_PARAMETERS),
    validate_group_options,
)
CONFIG_SCHEMA = setting_platform_schema(
    SELECT_PARAMETERS, ENTITY_SCHEMA, SINGLE_SCHEMA, GROUP_SCHEMA
)
FINAL_VALIDATE_SCHEMA = validate_setting_controller


async def to_code(config: ConfigType) -> None:
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    for entity in setting_configs(config, SELECT_PARAMETERS):
        spec = SELECT_PARAMETERS[entity[CONF_PARAMETER]]
        labels = [entity[CONF_OPTIONS][raw] for raw in sorted(entity[CONF_OPTIONS])]
        var = await select.new_select(
            entity, controller, spec.index, spec.minimum, options=labels
        )
        cg.add(var.initialize_options())
        register_setting_entity(controller, var)
