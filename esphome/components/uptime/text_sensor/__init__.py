import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_HOURS,
    CONF_ID,
    CONF_MINUTES,
    CONF_SECONDS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_TIMER,
)

uptime_ns = cg.esphome_ns.namespace("uptime")
UptimeTextSensor = uptime_ns.class_(
    "UptimeTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONF_SEPARATOR = "separator"
CONF_DAYS = "days"
CONF_EXPAND = "expand"

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        UptimeTextSensor,
        icon=ICON_TIMER,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.Optional(CONF_FORMAT, default={}): cv.Schema(
                {
                    cv.Optional(CONF_DAYS, default="d"): cv.string_strict,
                    cv.Optional(CONF_HOURS, default="h"): cv.string_strict,
                    cv.Optional(CONF_MINUTES, default="m"): cv.string_strict,
                    cv.Optional(CONF_SECONDS, default="s"): cv.string_strict,
                    cv.Optional(CONF_SEPARATOR, default=""): cv.string_strict,
                    cv.Optional(CONF_EXPAND, default=False): cv.boolean,
                }
            )
        }
    )
    .extend(cv.polling_component_schema("30s"))
)


async def to_code(config):
    format = config[CONF_FORMAT]
    var = cg.new_Pvariable(
        config[CONF_ID],
        format[CONF_DAYS],
        format[CONF_HOURS],
        format[CONF_MINUTES],
        format[CONF_SECONDS],
        format[CONF_SEPARATOR],
        format[CONF_EXPAND],
    )
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)
