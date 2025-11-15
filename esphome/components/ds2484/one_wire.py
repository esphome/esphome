import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_ID

ds2484_ns = cg.esphome_ns.namespace("ds2484")

CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"

CODEOWNERS = ["@mrk-its"]
DEPENDENCIES = ["i2c"]

DS2484OneWireBus = ds2484_ns.class_(
    "DS2484OneWireBus", OneWireBus, i2c.I2CDevice, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS2484OneWireBus),
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))
