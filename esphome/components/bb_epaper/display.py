from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_REFRESH,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

epaper_ns = cg.esphome_ns.namespace("bb_epaper")

bb_epaper = epaper_ns.class_(
    "bb_epaper", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.Schema(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(bb_epaper),
            cv.Required(CONF_MODEL): cv.string,
            cv.Optional(CONF_REFRESH): cv.string,
            cv.Optional(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_FULL_UPDATE_EVERY): cv.int_range(min=1, max=4294967295),
        }
    )
    .extend(cv.polling_component_schema("30s"))
    .extend(spi.spi_device_schema(cs_pin_required=True)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "bb_epaper", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_REFRESH in config:
        cg.add(var.set_refresh_type(config[CONF_REFRESH]))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_DC_PIN in config:
        dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
        cg.add(var.set_dc_pin(dc))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(busy))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    cg.add_library(
        name="bb_epaper",
        repository="https://github.com/bitbank2/bb_epaper.git",
        version=None,
    )
