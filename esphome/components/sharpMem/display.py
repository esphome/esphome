import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_WIDTH, CONF_HEIGHT, CONF_CS_PIN

AUTO_LOAD = ["display"]
CODEOWNERS = ["@skaldo"]
DEPENDENCIES = ["spi"]

sharpMem_ns = cg.esphome_ns.namespace("sharpMem")
SharpMem = sharpMem_ns.class_(
    "SharpMem", cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice
)
SharpMemRef = SharpMem.operator("ref")

CONF_DISP_PIN = "disp_pin"
CONF_EXTMODE_PIN = "extmode_pin"
CONF_EXTCOMIN_PIN = "extcomin_pin"
CONF_SHARP5V_ON_PIN = "sharp5v_on_pin"

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SharpMem),
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DISP_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_EXTMODE_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_EXTCOMIN_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_SHARP5V_ON_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_WIDTH): cv.int_,
            cv.Required(CONF_HEIGHT): cv.int_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(SharpMemRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    disp_pin = await cg.gpio_pin_expression(config[CONF_DISP_PIN])
    cg.add(var.set_disp_pin(disp_pin))
    extmode_pin = await cg.gpio_pin_expression(config[CONF_EXTMODE_PIN])
    cg.add(var.set_extmode_pin(extmode_pin))
    extcomin_pin = await cg.gpio_pin_expression(config[CONF_EXTCOMIN_PIN])
    cg.add(var.set_extcomin_pin(extcomin_pin))
    sharp5v_on_pin = await cg.gpio_pin_expression(config[CONF_SHARP5V_ON_PIN])
    cg.add(var.set_sharp5v_on_pin(sharp5v_on_pin))
    
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_height(config[CONF_HEIGHT]))
