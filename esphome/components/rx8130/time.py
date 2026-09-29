from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@beormund"]
DEPENDENCIES = ["i2c"]
rx8130_ns = cg.esphome_ns.namespace("rx8130")
RX8130Component = rx8130_ns.class_("RX8130Component", time.RealTimeClock, i2c.I2CDevice)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RX8130Component),
    }
).extend(i2c.i2c_device_schema(0x32))


RX8130_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(RX8130Component),
    }
)

automation.register_apply_action(
    "rx8130.write_time", RX8130_ACTION_SCHEMA, automation.ApplyCall("write_time()")
)

automation.register_apply_action(
    "rx8130.read_time", RX8130_ACTION_SCHEMA, automation.ApplyCall("read_time()")
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
