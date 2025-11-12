from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@beormund"]
DEPENDENCIES = ["i2c"]
rx8130_ns = cg.esphome_ns.namespace("rx8130")
RX8130Component = rx8130_ns.class_("RX8130Component", time.RealTimeClock, i2c.I2CDevice)
WriteAction = rx8130_ns.class_("WriteAction", automation.Action)
ReadAction = rx8130_ns.class_("ReadAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RX8130Component),
    }
).extend(i2c.i2c_device_schema(0x32))


@automation.register_action(
    "rx8130.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(RX8130Component),
        }
    ),
)
async def rx8130_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "rx8130.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(RX8130Component),
        }
    ),
)
async def rx8130_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
