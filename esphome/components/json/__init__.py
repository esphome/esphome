import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
json_ns = cg.esphome_ns.namespace("json")

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
)


@coroutine_with_priority(CoroPriority.BUS)
async def to_code(config):
    if CORE.is_esp32:
        from esphome.components.esp32 import add_idf_component

        add_idf_component(name="bblanchon/arduinojson", ref="7.4.2")
    else:
        cg.add_library("bblanchon/ArduinoJson", "7.4.2")
    cg.add_define("USE_JSON")
    cg.add_global(json_ns.using)
