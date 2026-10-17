from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@brogon"]
DEPENDENCIES = ["i2c"]
pcf85063_ns = cg.esphome_ns.namespace("pcf85063")
PCF85063Component = pcf85063_ns.class_(
    "PCF85063Component", time.RealTimeClock, i2c.I2CDevice
)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PCF85063Component),
    }
).extend(i2c.i2c_device_schema(0x51))


automation.register_apply_action(
    "pcf85063.write_time",
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(PCF85063Component),
        }
    ),
    automation.ApplyCall("write_time()"),
)

automation.register_apply_action(
    "pcf85063.read_time",
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(PCF85063Component),
        }
    ),
    automation.ApplyCall("read_time()"),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
