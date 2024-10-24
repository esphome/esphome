import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_SLEEP_PIN, CONF_TYPE
from esphome.components import i2c

CODEOWNERS = ["@alainstark"]

MULTI_CONF = True
DEPENDENCIES = ["i2c"]

CONF_BUS_SLEEP = "bus_sleep"
CONF_HUB_SLEEP = "hub_sleep"
CONF_ACTIVE_PULLUP = "active_pullup"
CONF_STRONG_PULLUP = "strong_pullup"

ds248x_ns = cg.esphome_ns.namespace("ds248x")
DS248xComponent = ds248x_ns.class_(
    "DS248xComponent", cg.PollingComponent, i2c.I2CDevice
)

ds248xType = ds248x_ns.enum("DS248xType", is_class=True)

DS248X_TYPES = {
    "ds2482-100": ds248xType.DS2482_100,
    "ds2482-800": ds248xType.DS2482_800,
}


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS248xComponent),
            cv.Required(CONF_TYPE): cv.enum(DS248X_TYPES, lower=True),
            cv.Optional(CONF_SLEEP_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_BUS_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_HUB_SLEEP, default=False): cv.boolean,
            cv.Optional(CONF_ACTIVE_PULLUP, default=False): cv.boolean,
            cv.Optional(CONF_STRONG_PULLUP, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_ds248x_type(config[CONF_TYPE]))
    cg.add(var.set_bus_sleep(config[CONF_BUS_SLEEP]))
    cg.add(var.set_hub_sleep(config[CONF_HUB_SLEEP]))
    cg.add(var.set_active_pullup(config[CONF_ACTIVE_PULLUP]))
    cg.add(var.set_strong_pullup(config[CONF_STRONG_PULLUP]))

    if CONF_SLEEP_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_SLEEP_PIN])
        cg.add(var.set_sleep_pin(pin))
