"""Confirmed enumerated controls for DeltaSol CS Plus 1.11."""

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
    require_controller_writes,
    vbus_ns,
)
from ..cs_plus_selects import SELECT_PARAMETERS

DEPENDENCIES = ["vbus"]
VBusSettingSelect = vbus_ns.class_("VBusSettingSelect", select.Select)


def validate_options(config: ConfigType) -> ConfigType:
    require_controller_writes(config[CONF_CONTROLLER_ID])
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


CONFIG_SCHEMA = cv.All(
    select.select_schema(
        VBusSettingSelect, entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(
        {
            cv.Required(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            cv.Required(CONF_PARAMETER): cv.one_of(*SELECT_PARAMETERS, lower=True),
            cv.Optional(CONF_OPTIONS): cv.Schema(
                {cv.int_: cv.All(cv.string_strict, cv.Length(min=1))}
            ),
        }
    ),
    validate_options,
)


async def to_code(config: ConfigType) -> None:
    spec = SELECT_PARAMETERS[config[CONF_PARAMETER]]
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    labels = [config[CONF_OPTIONS][raw] for raw in sorted(config[CONF_OPTIONS])]
    var = await select.new_select(
        config, controller, spec.index, spec.minimum, options=labels
    )
    cg.add(var.initialize_options())
    register_setting_entity(controller, var)
