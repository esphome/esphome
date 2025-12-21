from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID

DEPENDENCIES = ["i2c"]

I2C_ADDR = 0x51

bm8563_ns = cg.esphome_ns.namespace("bm8563")
BM8563 = bm8563_ns.class_("BM8563", time.RealTimeClock, i2c.I2CDevice)
WriteAction = bm8563_ns.class_("WriteAction", automation.Action)
ReadAction = bm8563_ns.class_("ReadAction", automation.Action)
TimerAction = bm8563_ns.class_("TimerAction", automation.Action)

CONFIG_SCHEMA = (
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BM8563),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(I2C_ADDR))
)


@automation.register_action(
    "bm8563.write_time",
    WriteAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
)
async def bm8563_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "bm8563.start_timer",
    TimerAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BM8563),
            cv.Required(CONF_DURATION): cv.templatable(cv.positive_time_period_seconds),
        }
    ),
)
async def bm8563_start_timer_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    template_ = await cg.templatable(config[CONF_DURATION], args, cg.uint32)
    cg.add(var.set_duration(template_))
    return var


@automation.register_action(
    "bm8563.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BM8563),
        }
    ),
)
async def bm8563_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
