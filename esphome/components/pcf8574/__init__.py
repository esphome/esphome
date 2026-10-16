from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_OUTPUT,
    CONF_RESET,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
DOMAIN = "pcf8574"
MULTI_CONF = True

pcf8574_ns = cg.esphome_ns.namespace("pcf8574")

PCF8574Component = pcf8574_ns.class_("PCF8574Component", cg.Component, i2c.I2CDevice)
PCF8574GPIOPin = pcf8574_ns.class_("PCF8574GPIOPin", cg.GPIOPin)

CONF_PCF8574 = "pcf8574"
CONF_PCF8575 = "pcf8575"
# Keep in sync with PCF8574Component::reset_ in pcf8574.h.
DEFAULT_RESET = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCF8574Component),
            cv.Optional(CONF_PCF8575, default=False): cv.boolean,
            cv.Optional(CONF_RESET, default=DEFAULT_RESET): cv.boolean,
            cv.Optional(CONF_INTERRUPT_PIN): gpio_expander.validate_interrupt_pin,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x21))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_pcf8575(config[CONF_PCF8575]))
    if (reset := config[CONF_RESET]) != DEFAULT_RESET:
        cg.add(var.set_reset(reset))
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))


PCF8574_PIN_SCHEMA = pins.gpio_base_schema(
    PCF8574GPIOPin,
    cv.int_range(min=0, max=15),
    modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=pins.gpio_validate_input_xor_output,
    invertible=True,
).extend(
    {
        cv.Required(CONF_PCF8574): pins.use_id_or_address(PCF8574Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCF8574, PCF8574_PIN_SCHEMA)
async def pcf8574_pin_to_code(config: ConfigType) -> MockObj:
    return await pins.new_parented_pin(config, CONF_PCF8574)
