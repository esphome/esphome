import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@dwmw2"]
DOMAIN = "crc32"

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CRC32")
