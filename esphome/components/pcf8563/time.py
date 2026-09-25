from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@KoenBreeman"]

DEPENDENCIES = ["i2c"]


pcf8563_ns = cg.esphome_ns.namespace("pcf8563")
pcf8563Component = pcf8563_ns.class_(
    "PCF8563Component", time.RealTimeClock, i2c.I2CDevice
)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(pcf8563Component),
    }
).extend(i2c.i2c_device_schema(0x51))


PCF8563_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(pcf8563Component),
    }
)

automation.register_apply_action(
    "pcf8563.write_time", PCF8563_ACTION_SCHEMA, automation.ApplyCall("write_time()")
)
automation.register_apply_action(
    "pcf8563.read_time", PCF8563_ACTION_SCHEMA, automation.ApplyCall("read_time()")
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
