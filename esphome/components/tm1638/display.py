from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLK_PIN,
    CONF_DIO_PIN,
    CONF_ID,
    CONF_INTENSITY,
    CONF_STB_PIN,
)
from esphome.types import ConfigType

CODEOWNERS = ["@skykingjwc"]

CONF_TM1638_ID = "tm1638_id"

tm1638_ns = cg.esphome_ns.namespace("tm1638")
TM1638Component = tm1638_ns.class_("TM1638Component", cg.PollingComponent)
TM1638ComponentRef = TM1638Component.operator("ref")

# Publishes the key binary sensors from the key bitmask read on each loop
KEYS_PUBLISHER = cg.HubFunction(
    "keys_fn",
    cg.void,
    [(cg.uint8, "keys")],
    setter="set_keys_publisher",
    define="USE_TM1638_KEYS_PUBLISHER",
)


CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638Component),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_STB_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DIO_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_INTENSITY, default=7): cv.int_range(min=0, max=8),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    await cg.add_pin(var.set_clk_pin, config[CONF_CLK_PIN])
    await cg.add_pin(var.set_dio_pin, config[CONF_DIO_PIN])
    await cg.add_pin(var.set_stb_pin, config[CONF_STB_PIN])

    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    await display.add_writer(var, config, TM1638ComponentRef)
