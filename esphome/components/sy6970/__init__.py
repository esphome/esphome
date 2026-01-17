import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@linkedupbits"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_SY6970_ID = "sy6970_id"
CONF_ENABLE_STATUS_LED = "enable_status_led"

sy6970_ns = cg.esphome_ns.namespace("sy6970")
SY6970Component = sy6970_ns.class_(
    "SY6970Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SY6970Component),
            cv.Optional(CONF_ENABLE_STATUS_LED, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x6A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if config[CONF_ENABLE_STATUS_LED]:
        cg.add(var.set_led_enabled(config[CONF_ENABLE_STATUS_LED]))
