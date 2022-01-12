import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import (
    CONF_AUTH,
    CONF_CLOSE_DURATION,
    CONF_ID,
    CONF_OPEN_DURATION,
    CONF_CHANNEL,
    CONF_IP_ADDRESS,
    CONF_PASSWORD
)

DEPENDENCIES = ["network"]
#AUTO_LOAD = ["json"]
CODEOWNERS = ["@icarome"]

myhomecover_ns = cg.esphome_ns.namespace("myhomecover")
MyHomeCover = myhomecover_ns.class_(
    "MyHomeCover", cover.Cover, cg.Component
)
CONF_NEEDS_AUTH = "require_auth"

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MyHomeCover),
            cv.Required(CONF_CHANNEL): cv.string,
            cv.Required(CONF_IP_ADDRESS): cv.string,
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_NEEDS_AUTH): cv.boolean,
            cv.Optional(CONF_PASSWORD, default=12345): cv.int_,
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
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_ip(config[CONF_IP_ADDRESS]))
    if CONF_NEEDS_AUTH in config:
        cg.add(var.set_auth(config[CONF_NEEDS_AUTH], config[CONF_PASSWORD]))
