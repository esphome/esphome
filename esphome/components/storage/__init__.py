import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@p1ngb4ck"]

storage_ns = cg.esphome_ns.namespace("storage")

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    pass
