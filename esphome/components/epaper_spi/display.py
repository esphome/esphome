import importlib
import pkgutil

from components.epaper_spi import models
from components.mipi import map_sequence

from esphome import core, pins
import esphome.codegen as cg
from esphome.components import display, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_ENABLE_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
    CONF_WIDTH,
)

AUTO_LOAD = ["split_buffer"]
DEPENDENCIES = ["spi"]

epaper_spi_ns = cg.esphome_ns.namespace("epaper_spi")
EPaperBase = epaper_spi_ns.class_(
    "EPaperBase", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

EPaperSpectraE6 = epaper_spi_ns.class_("EPaperSpectraE6", EPaperBase)
EPaper7p3InSpectraE6 = epaper_spi_ns.class_("EPaper7p3InSpectraE6", EPaperSpectraE6)


# Import all models dynamically from the models package
for module_info in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{module_info.name}", package=__package__)

MODELS = models.EpaperModel.models

DIMENSION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WIDTH): cv.int_,
        cv.Required(CONF_HEIGHT): cv.int_,
    }
)


def model_schema(config):
    model = MODELS[config[CONF_MODEL]]
    cv_dimensions = cv.Optional if model.get_default(CONF_WIDTH) else cv.Required
    return (
        display.FULL_DISPLAY_SCHEMA.extend(
            spi.spi_device_schema(
                cs_pin_required=False,
                default_mode="MODE0",
                default_data_rate=model.get_default(CONF_DATA_RATE, 10_000_000),
            )
        )
        .extend(
            {
                model.option(pin, cv.UNDEFINED): pins.gpio_output_pin_schema
                for pin in (CONF_RESET_PIN, CONF_CS_PIN, CONF_DC_PIN, CONF_BUSY_PIN)
            }
        )
        .extend(
            {
                cv.GenerateID(): cv.declare_id(model.class_name),
                cv_dimensions(CONF_DIMENSIONS): DIMENSION_SCHEMA,
                model.option(CONF_ENABLE_PIN, cv.UNDEFINED): cv.ensure_list(
                    pins.gpio_output_pin_schema
                ),
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                cv.Optional(CONF_INIT_SEQUENCE): cv.ensure_list(map_sequence),
                model.option(CONF_RESET_DURATION): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=core.TimePeriod(milliseconds=500)),
                ),
            }
        )
    )


def customise_schema(config):
    """
    Create a customised config schema for a specific model and validate the configuration.
    :param config: The configuration dictionary to validate
    :return: The validated configuration dictionary
    :raises cv.Invalid: If the configuration is invalid
    """
    # First get the model and bus mode
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    return model_schema(config)(config)


CONFIG_SCHEMA = customise_schema

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "epaper_spi", require_miso=False, require_mosi=True
)


async def to_code(config):
    model = MODELS[config[CONF_MODEL]]

    rhs = model.new()
    var = cg.Pvariable(config[CONF_ID], rhs, model)

    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
        cg.add(var.set_busy_pin(busy))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
