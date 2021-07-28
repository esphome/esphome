import esphome.codegen as cg
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@OttoWinter"]
json_ns = cg.esphome_ns.namespace("json")


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_library("ArduinoJson-esphomelib", "5.13.3")
    cg.add_define("USE_JSON")
    cg.add_global(json_ns.using)
