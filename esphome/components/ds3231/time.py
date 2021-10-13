import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time
from esphome.const import CONF_ID
from . import DS3231Component, ds3231_ns, CONF_DS3231_ID


DEPENDENCIES = ["ds3231"]

DS3231RTC = ds3231_ns.class_("DS3231RTC", time.RealTimeClock)
WriteTimeAction = ds3231_ns.class_("WriteTimeAction", automation.Action)
ReadTimeAction = ds3231_ns.class_("ReadTimeAction", automation.Action)

CONFIG_SCHEMA = cv.All(
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DS3231RTC),
            cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        }
    )
)


@automation.register_action(
    "ds3231.write_time",
    WriteTimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231RTC),
            cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        }
    ),
)
async def ds3231_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3231.read_time",
    ReadTimeAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3231RTC),
        }
    ),
)
async def ds3231_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    ds3231 = await cg.get_variable(config[CONF_DS3231_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, ds3231)
    await cg.register_component(var, config)
    await time.register_time(var, config)
