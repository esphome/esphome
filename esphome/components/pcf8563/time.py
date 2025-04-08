from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@KoenBreeman"]

DEPENDENCIES = ["i2c"]


pcf8563_ns = cg.esphome_ns.namespace("pcf8563")
pcf8563Component = pcf8563_ns.class_(
    "PCF8563Component", time.RealTimeClock, i2c.I2CDevice
)
WriteAction = pcf8563_ns.class_("WriteAction", automation.Action)
ReadAction = pcf8563_ns.class_("ReadAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(pcf8563Component),
    }
).extend(i2c.i2c_device_schema(0xA3))


@automation.register_action(
    "pcf8563.write_time",
    WriteAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(pcf8563Component),
        }
    ),
)
async def pcf8563_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "pcf8563.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(pcf8563Component),
        }
    ),
)
async def pcf8563_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
