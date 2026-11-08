import esphome.codegen as cg
from esphome.components import select
from esphome.components.const import CONF_STRING_DATAPOINT
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENUM_DATAPOINT,
    CONF_INT_DATAPOINT,
    CONF_OPTIMISTIC,
    CONF_OPTIONS,
)
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

from .. import CONF_TUYA_ID, Tuya, TuyaDatapointType, tuya_ns

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@bearpawmaxim"]

TuyaSelect = tuya_ns.class_("TuyaSelect", select.Select, cg.Component)


def datapoint_value(value: int) -> int:
    """Option key: the 0-255 datapoint value. A named function, because the
    schema dumper can only name bare-callable wildcard keys."""
    return cv.int_range(0, 2**8 - 1)(value)


OPTIONS_MAP_SCHEMA = cv.Schema({datapoint_value: cv.string_strict})


def ensure_option_map(value):
    cv.check_not_templatable(value)
    value = OPTIONS_MAP_SCHEMA(value)

    all_values = list(value.keys())
    unique_values = set(value.keys())
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Mapping values must be unique.")

    return value


def validate_options_shape(config: ConfigType) -> ConfigType:
    if CONF_STRING_DATAPOINT in config:
        if not isinstance(config[CONF_OPTIONS], list):
            raise cv.Invalid(
                f"With '{CONF_STRING_DATAPOINT}', '{CONF_OPTIONS}' must be a list of strings"
            )
    elif not isinstance(config[CONF_OPTIONS], dict):
        raise cv.Invalid(
            f"'{CONF_OPTIONS}' must be a mapping from datapoint value to option name"
        )
    return config


def validate_no_nul(value: str) -> str:
    """A string option is sent verbatim and matched with NUL-terminated
    strings at runtime, so an embedded NUL byte can never round-trip."""
    if "\0" in value:
        raise cv.Invalid("Option must not contain NUL bytes")
    return value


@schema_extractor("schema")
def validate_options(value: ConfigType | list) -> ConfigType | list | cv.Schema:
    """Mapping from datapoint values to option names (enum/int datapoints), or
    a plain list of option names (string datapoints).

    The language-schema dumper cannot represent a union of shapes; merging the
    cv.Any branches into one entry advertised a bare string list for every
    select, so expose the mapping form, which the dumper can walk.
    """
    if value is SCHEMA_EXTRACT:
        return OPTIONS_MAP_SCHEMA
    return cv.Any(
        ensure_option_map,
        cv.All(cv.ensure_list(cv.string_strict, validate_no_nul), cv.Length(min=1)),
    )(value)


CONFIG_SCHEMA = cv.All(
    select.select_schema(TuyaSelect)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Optional(CONF_ENUM_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_INT_DATAPOINT): cv.uint8_t,
            cv.Optional(CONF_STRING_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_OPTIONS): validate_options,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(
        CONF_ENUM_DATAPOINT, CONF_INT_DATAPOINT, CONF_STRING_DATAPOINT
    ),
    validate_options_shape,
)


async def to_code(config):
    if CONF_STRING_DATAPOINT in config:
        # For string_datapoint, options is a list of strings sent verbatim
        options = config[CONF_OPTIONS]
        mappings = []
    else:
        # For enum/int_datapoint, options is a mapping from datapoint values
        options_map = config[CONF_OPTIONS]
        options = list(options_map.values())
        mappings = list(options_map.keys())

    var = await select.new_select(config, options=options)
    await cg.register_component(var, config)
    cg.add(var.set_select_mappings(mappings))
    await cg.add_variable(var.set_tuya_parent, config[CONF_TUYA_ID])
    if (string_datapoint := config.get(CONF_STRING_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(string_datapoint, TuyaDatapointType.STRING))
    if (enum_datapoint := config.get(CONF_ENUM_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(enum_datapoint, TuyaDatapointType.ENUM))
    if (int_datapoint := config.get(CONF_INT_DATAPOINT, None)) is not None:
        cg.add(var.set_select_id(int_datapoint, TuyaDatapointType.INTEGER))
    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
