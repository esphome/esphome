import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, globals
from esphome.const import (
    CONF_CLOSE_DURATION,
    CONF_ID,
    CONF_OPEN_DURATION,
)
CODEOWNERS = ["@icarome"]

somfycover_ns = cg.esphome_ns.namespace("somfycover")
SomfyCover = somfycover_ns.class_(
    "SomfyCover", cover.Cover, cg.Component
)
CONF_REMOTE_ID = "remote_id"
CONF_ROLLING_CODE = "rolling_code_id"

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SomfyCover),
            cv.Required(CONF_REMOTE_ID): cv.hex_uint32_t,
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_ROLLING_CODE): cv.use_id(globals.GlobalsComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    cg.add(var.set_remote_id(config[CONF_REMOTE_ID]))
    rolling_code_id = await cg.get_variable(config[CONF_ROLLING_CODE])
    cg.add(var.set_rolling_code_(rolling_code_id))
