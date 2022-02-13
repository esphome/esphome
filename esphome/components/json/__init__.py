import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@OttoWinter"]
json_ns = cg.esphome_ns.namespace("json")

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_library("bblanchon/ArduinoJson", "6.18.5")
    cg.add_define("USE_JSON")
    cg.add_global(json_ns.using)
