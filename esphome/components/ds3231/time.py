import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time
from esphome.const import CONF_ID


CODEOWNERS = ["@nuttytree"]
DEPENDENCIES = ["ds3231"]

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231RTC = ds3231_ns.class_("DS3231RTC", time.RealTimeClock)
WriteTimeAction = ds3231_ns.class_("WriteTimeAction", automation.Action)
ReadTimeAction = ds3231_ns.class_("ReadTimeAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS3231RTC),
    }
)


@automation.register_action(
    "ds3231.write_time",
    WriteTimeAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3231RTC),
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
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await time.register_time(var, config)
