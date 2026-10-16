from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INPUT, CONF_INTERRUPT_PIN, CONF_OUTPUT
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@ssieb", "@ajfriesen"]
DEPENDENCIES = ["i2c"]
DOMAIN = "pca9505"
MULTI_CONF = True

pca9505_ns = cg.esphome_ns.namespace("pca9505")

PCA9505Component = pca9505_ns.class_("PCA9505Component", cg.Component, i2c.I2CDevice)
PCA9505GPIOPin = pca9505_ns.class_(
    "PCA9505GPIOPin", cg.GPIOPin, cg.Parented.template(PCA9505Component)
)

CONF_PCA9505 = "pca9505"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9505Component),
            cv.Optional(CONF_INTERRUPT_PIN): gpio_expander.validate_interrupt_pin,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))


PCA9505_PIN_SCHEMA = pins.gpio_base_schema(
    PCA9505GPIOPin,
    cv.int_range(min=0, max=39),
    modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=pins.gpio_validate_input_xor_output,
).extend(
    {
        cv.Required(CONF_PCA9505): cv.use_id(PCA9505Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCA9505, PCA9505_PIN_SCHEMA)
async def pca9505_pin_to_code(config: ConfigType) -> MockObj:
    return await pins.new_parented_pin(config, CONF_PCA9505)
