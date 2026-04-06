from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE

from . import generate, schema

CONF_AUTO_MIN_VALUE = "auto_min_value"
CONF_AUTO_MAX_VALUE = "auto_max_value"

OpenthermInput = generate.opentherm_ns.class_("OpenthermInput")


def validate_min_value_less_than_max_value(conf):
    if (
        CONF_MIN_VALUE in conf
        and CONF_MAX_VALUE in conf
        and conf[CONF_MIN_VALUE] > conf[CONF_MAX_VALUE]
    ):
        raise cv.Invalid(f"{CONF_MIN_VALUE} must be less than {CONF_MAX_VALUE}")
    return conf


def input_schema(entity: schema.InputSchema) -> cv.Schema:
    result = cv.Schema(
        {
            cv.Optional(CONF_MIN_VALUE, entity.range[0]): cv.float_range(
                entity.range[0], entity.range[1]
            ),
            cv.Optional(CONF_MAX_VALUE, entity.range[1]): cv.float_range(
                entity.range[0], entity.range[1]
            ),
        }
    )
    result = result.add_extra(validate_min_value_less_than_max_value)
    if entity.auto_min_value is not None:
        result = result.extend({cv.Optional(CONF_AUTO_MIN_VALUE, False): cv.boolean})
    if entity.auto_max_value is not None:
        result = result.extend({cv.Optional(CONF_AUTO_MAX_VALUE, False): cv.boolean})

    return result


def generate_setters(entity: cg.MockObj, conf: dict[str, Any]) -> None:
    generate.add_property_set(entity, CONF_MIN_VALUE, conf)
    generate.add_property_set(entity, CONF_MAX_VALUE, conf)
    generate.add_property_set(entity, CONF_AUTO_MIN_VALUE, conf)
    generate.add_property_set(entity, CONF_AUTO_MAX_VALUE, conf)
