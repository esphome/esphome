import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@mschnaubelt"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_AXP2101_ID = "axp2101_id"
CONF_ICC_LIMIT = "icc_limit"

axp2101_ns = cg.esphome_ns.namespace("axp2101")
AXP2101Component = axp2101_ns.class_(
    "AXP2101Component", cg.PollingComponent, i2c.I2CDevice
)

ICC_LIMIT_OPTIONS = {
    "0mA": 0x00,
    "25mA": 0x01,
    "50mA": 0x02,
    "75mA": 0x03,
    "100mA": 0x04,
    "125mA": 0x05,
    "150mA": 0x06,
    "175mA": 0x07,
    "200mA": 0x08,
    "300mA": 0x09,
    "400mA": 0x0A,
    "500mA": 0x0B,
    "600mA": 0x0C,
    "700mA": 0x0D,
    "800mA": 0x0E,
    "900mA": 0x0F,
    "1000mA": 0x10,
    "1100mA": 0x11,
    "1200mA": 0x12,
    "1300mA": 0x13,
    "1400mA": 0x14,
    "1500mA": 0x15,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AXP2101Component),
            cv.Optional(CONF_ICC_LIMIT, default="100mA"): cv.enum(ICC_LIMIT_OPTIONS),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x34))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_icc_limit(config[CONF_ICC_LIMIT]))
