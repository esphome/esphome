from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_OUTPUT, CONF_PULLDOWN, CONF_PULLUP
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@ssieb"]
DEPENDENCIES = ["i2c"]
DOMAIN = "seesaw"
MULTI_CONF = True

seesaw_ns = cg.esphome_ns.namespace("seesaw")
Seesaw = seesaw_ns.class_("Seesaw", i2c.I2CDevice, cg.Component)
SeesawGPIOPin = seesaw_ns.class_(
    "SeesawGPIOPin", cg.GPIOPin, cg.Parented.template(Seesaw)
)

CONF_SEESAW = "seesaw"
CONF_SEESAW_ID = "seesaw_id"

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Seesaw),
    }
).extend(i2c.i2c_device_schema(0x49))


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)


SEESAW_PIN_SCHEMA = pins.gpio_base_schema(
    SeesawGPIOPin,
    cv.int_range(min=0, max=31),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN],
).extend(
    {
        cv.Required(CONF_SEESAW): cv.use_id(Seesaw),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SEESAW, SEESAW_PIN_SCHEMA)
async def seesaw_pin_to_code(config: ConfigType) -> cg.MockObj:
    return await pins.new_parented_pin(config, CONF_SEESAW)
