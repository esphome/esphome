import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_CS_PIN,
    CONF_BUSY_PIN,
)

DEPENDENCIES = ["spi"]

gdew0154m09_ns = cg.esphome_ns.namespace("gdew0154m09")
GDEW0154M09 = gdew0154m09_ns.class_(
    "GDEW0154M09", cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice
)


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GDEW0154M09),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))
    busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
