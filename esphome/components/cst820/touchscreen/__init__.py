import esphome.config_validation as cv
import esphome.codegen as cg


from esphome.components import touchscreen, i2c
from esphome.const import CONF_ID

from .. import CST820_ns

DEPENDENCIES = ["i2c"]

CST820Component = CST820_ns.class_(
    "CST820Touchscreen",
    touchscreen.Touchscreen,
    cg.PollingComponent,
    i2c.I2CDevice,
)


def validate_cst820(config):
    return config


def report_interval(value):
    if value == "never":
        return 4294967295  # uint32_t max
    return cv.positive_time_period_milliseconds(value)


CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CST820Component),
        },
    )
    .extend(i2c.i2c_device_schema(0x15)),
).add_extra(validate_cst820)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    # await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
