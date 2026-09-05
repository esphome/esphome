from esphome import automation
import esphome.codegen as cg
from esphome.components import time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

DS3231Time = ds3231_ns.class_(
    "DS3231Time", time.RealTimeClock, cg.Parented.template(DS3231Component)
)
WriteAction = ds3231_ns.class_("WriteAction", automation.Action)
ReadAction = ds3231_ns.class_("ReadAction", automation.Action)

CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS3231Time),
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
    }
)


@automation.register_action(
    "ds3231.write_time",
    WriteAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3231Time),
        }
    ),
    synchronous=True,
)
async def ds3231_write_time_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3231.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS3231Time),
        }
    ),
    synchronous=True,
)
async def ds3231_read_time_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3231_ID])
    await time.register_time(var, config)
