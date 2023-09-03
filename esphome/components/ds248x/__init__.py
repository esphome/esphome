import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_SLEEP_PIN
from esphome.components import i2c

MULTI_CONF = True
AUTO_LOAD = ["sensor"]
DEPENDENCIES = ["i2c"]

CONF_BUS_SLEEP = "bus_sleep"
CONF_HUB_SLEEP = "hub_sleep"
CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"
CONF_CHANNEL_COUNT = "channel_count"

ds248x_ns = cg.esphome_ns.namespace("ds248x")
DS248xComponent = ds248x_ns.class_(
    "DS248xComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS248xComponent),
            cv.Optional(CONF_SLEEP_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_BUS_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_HUB_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_CHANNEL_COUNT, default=1): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_bus_sleep(config[CONF_BUS_SLEEP]))
    cg.add(var.set_hub_sleep(config[CONF_HUB_SLEEP]))
    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))
    cg.add(var.set_channel_count(config[CONF_CHANNEL_COUNT]))

    if CONF_SLEEP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(pin))
