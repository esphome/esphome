"""Test only: drives PA0/PA1, reads IOCON and the INT lines, and checks the PB binary sensors."""

from esphome import pins
import esphome.codegen as cg
from esphome.components import binary_sensor, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
DOMAIN = "mcptest"
mcptest_ns = cg.esphome_ns.namespace("mcptest")
McpTest = mcptest_ns.class_("McpTest", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(McpTest),
            cv.Required("pa0"): pins.gpio_output_pin_schema,
            cv.Required("pa1"): pins.gpio_output_pin_schema,
            cv.Required("pb0"): cv.use_id(binary_sensor.BinarySensor),
            cv.Required("pb1"): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional("isr_inta", default=False): cv.boolean,
            cv.Optional("label", default=""): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x27))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(
        var.set_pins(
            await cg.gpio_pin_expression(config["pa0"]),
            await cg.gpio_pin_expression(config["pa1"]),
        )
    )
    cg.add(
        var.set_bs(
            await cg.get_variable(config["pb0"]), await cg.get_variable(config["pb1"])
        )
    )
    cg.add(var.set_opts(config["isr_inta"], config["label"]))
