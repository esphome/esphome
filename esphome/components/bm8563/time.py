from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

I2C_ADDR = 0x51

bm8563_ns = cg.esphome_ns.namespace("bm8563")
BM8563 = bm8563_ns.class_("BM8563", time.RealTimeClock, i2c.I2CDevice)

CONFIG_SCHEMA = (
    time.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BM8563),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(I2C_ADDR))
)


BM8563_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(BM8563),
    }
)

automation.register_apply_action(
    "bm8563.write_time", BM8563_ACTION_SCHEMA, automation.ApplyCall("write_time()")
)

automation.register_apply_action(
    "bm8563.read_time", BM8563_ACTION_SCHEMA, automation.ApplyCall("read_time()")
)

automation.register_apply_action(
    "bm8563.start_timer",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BM8563),
            cv.Required(CONF_DURATION): cv.templatable(cv.positive_time_period_seconds),
        }
    ),
    automation.ApplyField(CONF_DURATION, "start_timer", cg.uint32),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
