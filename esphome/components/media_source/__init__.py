import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["media_player"]

IS_PLATFORM_COMPONENT = True

media_source_ns = cg.esphome_ns.namespace("media_source")

MediaSource = media_source_ns.class_("MediaSource")


async def register_media_source(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    CORE.register_platform_component("media_source", var)


async def new_media_source(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_media_source(var, config)
    return var


_MEDIA_SOURCE_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend({})


def media_source_schema(
    class_: MockObjClass,
    *,
    media_player,
) -> cv.Schema:
    schema = {cv.GenerateID(CONF_ID): cv.declare_id(class_)}

    return _MEDIA_SOURCE_SCHEMA.extend(schema)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(media_source_ns.using)
    cg.add_define("USE_MEDIA_SOURCE")
