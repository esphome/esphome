import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_WIDTH, CONF_HEIGHT

CODEOWNERS = ["@skaldo"]
DEPENDENCIES = ["spi"]

sharp_memory_lcd_ns = cg.esphome_ns.namespace("sharp_memory_lcd")
SharpMemoryLCD = sharp_memory_lcd_ns.class_(
    "SharpMemoryLCD", cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice
)
SharpMemoryLCDRef = SharpMemoryLCD.operator("ref")

CONF_DISP_PIN = "disp_pin"
CONF_EXTMODE_PIN = "extmode_pin"
CONF_EXTCOMIN_PIN = "extcomin_pin"
CONF_INVERT_COLOR = "invert_color"

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SharpMemoryLCD),
            cv.Optional(CONF_DISP_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_EXTMODE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_EXTCOMIN_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_WIDTH): cv.int_,
            cv.Required(CONF_HEIGHT): cv.int_,
            cv.Optional(CONF_INVERT_COLOR, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(SharpMemoryLCDRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_DISP_PIN in config:
        disp_pin = await cg.gpio_pin_expression(config[CONF_DISP_PIN])
        cg.add(var.set_disp_pin(disp_pin))
    if CONF_EXTMODE_PIN in config:
        extmode_pin = await cg.gpio_pin_expression(config[CONF_EXTMODE_PIN])
        cg.add(var.set_extmode_pin(extmode_pin))
    if CONF_EXTCOMIN_PIN in config:
        extcomin_pin = await cg.gpio_pin_expression(config[CONF_EXTCOMIN_PIN])
        cg.add(var.set_extcomin_pin(extcomin_pin))

    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))

    cg.add(var.set_invert_color(config[CONF_INVERT_COLOR]))
