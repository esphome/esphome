import esphome.codegen as cg
from esphome.core import coroutine_with_priority

IS_PLATFORM_COMPONENT = True


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_library("throwtheswitch/Unity", "^2.5.2")
