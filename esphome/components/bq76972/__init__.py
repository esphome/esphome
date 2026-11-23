from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS
from esphome.types import ConfigType

CODEOWNERS = ["@limpkin"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bq76972"
MULTI_CONF = True

bq76972_ns = cg.esphome_ns.namespace("bq76972")
BQ76972Component = bq76972_ns.class_(
    "BQ76972Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_REG_DISABLED = "reg_disabled"
CONF_CRC_ENABLED = "crc_enabled"
CONF_BQ76972_ID = "bq76972_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ76972Component),
            cv.Optional(CONF_CRC_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_REG_DISABLED, default=False): cv.boolean,
        }
    )
    .extend(i2c.i2c_device_schema(0x08))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_crc_mode(config[CONF_CRC_ENABLED]))
    cg.add(var.set_reg_disable(config[CONF_REG_DISABLED]))


BQ76972_PROGRAM_I2C_ADDRESS_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(BQ76972Component),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.i2c_address),
    },
    key=CONF_ADDRESS,
)

automation.register_apply_action(
    "bq76972.program_i2c_address",
    BQ76972_PROGRAM_I2C_ADDRESS_SCHEMA,
    automation.ApplyField(CONF_ADDRESS, "program_i2c_address", cg.uint8),
)
