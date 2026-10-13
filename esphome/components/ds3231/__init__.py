from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@linkedupbits"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
DOMAIN = "ds3231"

CONF_DS3231_ID = "ds3231_id"

ds3231_ns = cg.esphome_ns.namespace("ds3231")
DS3231Component = ds3231_ns.class_("DS3231Component", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(DS3231Component)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


automation.register_apply_action(
    "ds3231.force_temperature_conversion",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(DS3231Component)}),
    automation.ApplyCall("force_temperature_conversion()"),
)
