import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@DT-art1"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_define("USE_CAMERA")
