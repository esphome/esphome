from collections.abc import Callable
import difflib

import esphome.codegen as cg
from esphome.components.const import KEY_METADATA
import esphome.config_validation as cv
from esphome.const import CONF_FROM, CONF_ID, CONF_TO
from esphome.core import CORE, ID
from esphome.cpp_generator import (
    MockObj,
    MockObjClass,
    VariableDeclarationExpression,
    add_global,
)
from esphome.loader import get_component

CODEOWNERS = ["@clydebarrow"]
MULTI_CONF = True
DOMAIN = "mapping"

mapping_ns = cg.esphome_ns.namespace("mapping")
mapping_class = mapping_ns.class_("Mapping")

CONF_DEFAULT_VALUE = "default_value"
CONF_ENTRIES = "entries"
CONF_CLASS = "class"


class IndexType:
    """
    Represents a type of index in a map.
    """

    def __init__(
        self, validator: Callable, data_type: MockObj, conversion: Callable = None
    ) -> None:
        self.validator = validator
        self.data_type = data_type
        self.conversion = conversion

    async def convert_value(self, value):
        if self.conversion:
            return self.conversion(value)
        return await cg.get_variable(value)


INDEX_TYPES = {
    "int": IndexType(cv.int_, cg.int_, int),
    "string": IndexType(
        cv.string,
        cg.std_string,
        str,
    ),
}


class MappingMetaData:
    def __init__(self, from_: IndexType, to_: IndexType) -> None:
        self.from_ = from_
        self.to_ = to_


def to_schema(value):
    """
    Generate a schema for the 'to' field of a map. This can be either one of the index types or a class name.
    :param value:
    :return:
    """
    return cv.Any(
        cv.one_of(*INDEX_TYPES, lower=True),
        cv.one_of(*CORE.id_classes.keys()),
    )(value)


BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(mapping_class),
        cv.Required(CONF_FROM): cv.one_of(*INDEX_TYPES, lower=True),
        cv.Required(CONF_TO): cv.string,
    },
    extra=cv.ALLOW_EXTRA,
)


def get_object_type(to_) -> MockObjClass | None:
    """
    Get the object type from a string. Possible formats:
       xxx The name of a component which defines INSTANCE_TYPE
       esphome::xxx::yyy A C++ class name defined in a component
       xxx::yyy A C++ class name defined in a component
       yyy A C++ class name defined in the core
    """

    if cls := CORE.id_classes.get(to_):
        return cls
    if cls := CORE.id_classes.get(to_.removeprefix("esphome::")):
        return cls
    # get_component will throw a wobbly if we don't check this first.
    if "." in to_:
        return None
    if component := get_component(to_):
        return component.instance_type
    return None


def get_all_mapping_metadata() -> dict[str, MappingMetaData]:
    """Get all mapping metadata."""
    return CORE.data.setdefault(DOMAIN, {}).setdefault(KEY_METADATA, {})


def get_mapping_metadata(mapping_id: str) -> MappingMetaData:
    """Get mapping metadata by ID for use by other components."""
    return get_all_mapping_metadata()[mapping_id]


def add_metadata(
    mapping_id: ID,
    from_: IndexType,
    to_: IndexType,
) -> None:
    get_all_mapping_metadata()[mapping_id.id] = MappingMetaData(from_, to_)


def map_schema(config):
    config = BASE_SCHEMA(config)
    if CONF_ENTRIES not in config or not isinstance(config[CONF_ENTRIES], dict):
        raise cv.Invalid("an entries dictionary is required for a mapping")
    entries = config[CONF_ENTRIES]
    if len(entries) == 0:
        raise cv.Invalid("A mapping must have at least one entry")
    to_ = config[CONF_TO]
    if to_ in INDEX_TYPES:
        value_type = INDEX_TYPES[to_]
    else:
        object_type = get_object_type(to_)
        if object_type is None:
            matches = difflib.get_close_matches(to_, CORE.id_classes)
            raise cv.Invalid(
                f"No known mappable class name matches '{to_}'; did you mean one of {', '.join(matches)}?"
            )
        validator = cv.use_id(object_type)
        value_type = IndexType(validator, object_type)
    config[CONF_ENTRIES] = {k: value_type.validator(v) for k, v in entries.items()}
    if (default_value := config.get(CONF_DEFAULT_VALUE)) is not None:
        config[CONF_DEFAULT_VALUE] = value_type.validator(default_value)
    unexpected_keys = config.keys() - {
        CONF_ENTRIES,
        CONF_TO,
        CONF_FROM,
        CONF_ID,
        CONF_DEFAULT_VALUE,
    }
    if unexpected_keys:
        errors = [
            cv.Invalid(f"Unexpected key '{k}'", path=[k]) for k in unexpected_keys
        ]
        raise cv.MultipleInvalid(errors)

    add_metadata(config[CONF_ID], INDEX_TYPES[config[CONF_FROM]], value_type)
    return config


CONFIG_SCHEMA = map_schema


async def to_code(config):
    varid = config[CONF_ID]
    metadata = get_mapping_metadata(varid.id)
    entries = {
        metadata.from_.conversion(key): await metadata.to_.convert_value(value)
        for key, value in config[CONF_ENTRIES].items()
    }
    value_type = metadata.to_.data_type
    # entries guaranteed to be non-empty here.
    value_0 = list(entries.values())[0]
    if isinstance(value_0, MockObj) and value_0.op != ".":
        value_type = value_type.operator("ptr")
    varid.type = mapping_class.template(
        metadata.from_.data_type,
        value_type,
    )
    var = MockObj(varid, ".")
    decl = VariableDeclarationExpression(varid.type, "", varid, static=True)
    add_global(decl)
    CORE.register_variable(varid, var)

    for key, value in entries.items():
        cg.add(var.set(key, value))
    if (default_value := config.get(CONF_DEFAULT_VALUE)) is not None:
        cg.add(var.set_default_value(await metadata.to_.convert_value(default_value)))
    return var
