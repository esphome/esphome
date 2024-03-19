from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_INTENSITY, CONF_LAMBDA


DEPENDENCIES = ["spi", "esp32"]
CODEOWNERS = ["@endym"]
CONF_LOAD_PIN = "load_pin"
CONF_BLANK_PIN = "blank_pin"
CONF_NUM_DIGITS = "num_digits"

max6921_ns = cg.esphome_ns.namespace("max6921")
MAX6921Component = max6921_ns.class_(
    "MAX6921Component", cg.PollingComponent, spi.SPIDevice
)
MAX6921ComponentRef = MAX6921Component.operator("ref")

CONFIG_SCHEMA = (
    display.BASIC_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MAX6921Component),
            cv.Required(CONF_LOAD_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_BLANK_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_NUM_DIGITS): cv.int_range(min=1, max=20),
            cv.Optional(CONF_INTENSITY, default=16): cv.int_range(min=0, max=16),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=False))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await spi.register_spi_device(var, config)
    await display.register_display(var, config)

    load_pin = await cg.gpio_pin_expression(config[CONF_LOAD_PIN])
    cg.add(var.set_load_pin(load_pin))
    blank_pin = await cg.gpio_pin_expression(config[CONF_BLANK_PIN])
    cg.add(var.set_blank_pin(blank_pin))
    cg.add(var.set_num_digits(config[CONF_NUM_DIGITS]))
    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MAX6921ComponentRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
