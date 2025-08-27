import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_TYPE_ID
from esphome.util import Registry

EXTENSIONS_REGISTRY = Registry()
validate_extension = cv.validate_registry_entry("extension", EXTENSIONS_REGISTRY)
validate_extension_list = cv.validate_registry("extension", EXTENSIONS_REGISTRY)


def register_extension(name, extension_type, schema):
    return EXTENSIONS_REGISTRY.register(name, extension_type, schema)


async def build_extension(full_config, template_arg, args):
    registry_entry, config = cg.extract_registry_entry_config(
        EXTENSIONS_REGISTRY, full_config
    )
    extension_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    return await builder(config, extension_id, template_arg, args)


async def build_extension_list(config, templ, arg_type):
    extensions = []
    for conf in config:
        extension = await build_extension(conf, templ, arg_type)
        extensions.append(extension)
    return extensions
