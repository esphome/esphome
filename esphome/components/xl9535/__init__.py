from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INPUT, CONF_OUTPUT
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CONF_XL9535 = "xl9535"

AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@mreditor97"]
DOMAIN = "xl9535"

xl9535_ns = cg.esphome_ns.namespace(CONF_XL9535)

XL9535Component = xl9535_ns.class_("XL9535Component", cg.Component, i2c.I2CDevice)
XL9535GPIOPin = xl9535_ns.class_("XL9535GPIOPin", cg.GPIOPin)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema({cv.Required(CONF_ID): cv.declare_id(XL9535Component)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)


def validate_pin(pin: int) -> int:
    if pin in (8, 9):
        raise cv.Invalid(f"pin {pin} doesn't exist")
    return pin


XL9535_PIN_SCHEMA = pins.gpio_base_schema(
    XL9535GPIOPin,
    cv.All(cv.int_range(min=0, max=17), validate_pin),
    modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=pins.gpio_validate_input_xor_output,
).extend(
    {
        cv.Required(CONF_XL9535): pins.use_id_or_address(XL9535Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_XL9535, XL9535_PIN_SCHEMA)
async def xl9535_pin_to_code(config: ConfigType) -> MockObj:
    return await pins.new_parented_pin(config, CONF_XL9535)
