import difflib

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_FROM, CONF_ID, CONF_TO
from esphome.core import CORE
from esphome.cpp_generator import MockObj, VariableDeclarationExpression, add_global
from esphome.loader import get_component

CODEOWNERS = ["@clydebarrow"]
MULTI_CONF = True

map_ = cg.std_ns.class_("map")

CONF_ENTRIES = "entries"
CONF_CLASS = "class"


class IndexType:
    """
    Represents a type of index in a map.
    """

    def __init__(self, validator, data_type, conversion):
        self.validator = validator
        self.data_type = data_type
        self.conversion = conversion


INDEX_TYPES = {
    "int": IndexType(cv.int_, cg.int_, int),
    "string": IndexType(cv.string, cg.std_string, str),
}


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
        cv.Required(CONF_ID): cv.declare_id(map_),
        cv.Required(CONF_FROM): cv.one_of(*INDEX_TYPES, lower=True),
        cv.Required(CONF_TO): cv.string,
    },
    extra=cv.ALLOW_EXTRA,
)


def get_object_type(to_):
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


def map_schema(config):
    config = BASE_SCHEMA(config)
    if CONF_ENTRIES not in config or not isinstance(config[CONF_ENTRIES], dict):
        raise cv.Invalid("an entries list is required for a map")
    entries = config[CONF_ENTRIES]
    if len(entries) == 0:
        raise cv.Invalid("Map must have at least one entry")
    to_ = config[CONF_TO]
    if to_ in INDEX_TYPES:
        value_type = INDEX_TYPES[to_].validator
    else:
        value_type = get_object_type(to_)
        if value_type is None:
            matches = difflib.get_close_matches(to_, CORE.id_classes)
            raise cv.Invalid(
                f"No known mappable class name matches '{to_}'; did you mean one of {', '.join(matches)}?"
            )
        value_type = cv.use_id(value_type)
    config[CONF_ENTRIES] = {k: value_type(v) for k, v in entries.items()}
    return config


CONFIG_SCHEMA = map_schema


async def to_code(config):
    entries = config[CONF_ENTRIES]
    from_ = config[CONF_FROM]
    to_ = config[CONF_TO]
    index_conversion = INDEX_TYPES[from_].conversion
    index_type = INDEX_TYPES[from_].data_type
    if to_ in INDEX_TYPES:
        value_conversion = INDEX_TYPES[to_].conversion
        value_type = INDEX_TYPES[to_].data_type
        entries = {
            index_conversion(key): value_conversion(value)
            for key, value in entries.items()
        }
    else:
        entries = {
            index_conversion(key): await cg.get_variable(value)
            for key, value in entries.items()
        }
        value_type = get_object_type(to_)
        if list(entries.values())[0].op != ".":
            value_type = value_type.operator("ptr")
    varid = config[CONF_ID]
    varid.type = map_.template(index_type, value_type)
    var = MockObj(varid, ".")
    decl = VariableDeclarationExpression(varid.type, "", varid)
    add_global(decl)
    CORE.register_variable(varid, var)

    for key, value in entries.items():
        cg.add(var.insert((key, value)))
    return var
