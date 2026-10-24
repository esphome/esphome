from esphome import automation
import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

DS3231Time = ds3231_ns.class_(
    "DS3231Time", time.RealTimeClock, cg.Parented.template(DS3231Component)
)

CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS3231Time),
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
    }
)

DS3231_TIME_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(DS3231Time)}
)

automation.register_apply_action(
    "ds3231.write_time", DS3231_TIME_ACTION_SCHEMA, automation.ApplyCall("write_time()")
)

automation.register_apply_action(
    "ds3231.read_time", DS3231_TIME_ACTION_SCHEMA, automation.ApplyCall("read_time()")
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3231_ID])
    await time.register_time(var, config)
