import esphome.codegen as cg

CODEOWNERS = ["@esphome/core"]


async def to_code(config):
    cg.add_define("USE_MD5")
