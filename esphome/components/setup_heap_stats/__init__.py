import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@bdraco"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on(["esp32", "esp8266", "rp2040", "bk72xx", "rtl87xx", "ln882x"]),
)


async def to_code(config):
    cg.add_define("USE_SETUP_HEAP_STATS")
