import esphome.codegen as cg
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]


async def to_code(config):
    cg.add_define("USE_MD5")
    if CORE.is_host:
        cg.add_build_flag("-lcrypto")
