import importlib
import pkgutil

from esphome import core, pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.mipi import flatten_sequence, map_sequence
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

from . import models

AUTO_LOAD = ["split_buffer"]
DEPENDENCIES = ["spi"]

CONF_INIT_SEQUENCE_ID = "init_sequence_id"

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
    class_name = epaper_spi_ns.class_(model.class_name, EPaperBase)
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
                model.option(pin): pins.gpio_output_pin_schema
                for pin in (CONF_RESET_PIN, CONF_CS_PIN, CONF_BUSY_PIN)
            }
        )
        .extend(
            {
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                model.option(CONF_DC_PIN, fallback=None): pins.gpio_output_pin_schema,
                cv.GenerateID(): cv.declare_id(class_name),
                cv.GenerateID(CONF_INIT_SEQUENCE_ID): cv.declare_id(cg.uint8),
                cv_dimensions(CONF_DIMENSIONS): DIMENSION_SCHEMA,
                model.option(CONF_ENABLE_PIN): cv.ensure_list(
                    pins.gpio_output_pin_schema
                ),
                model.option(CONF_INIT_SEQUENCE, cv.UNDEFINED): cv.ensure_list(
                    map_sequence
                ),
                model.option(CONF_RESET_DURATION, cv.UNDEFINED): cv.All(
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

    init_sequence = config.get(CONF_INIT_SEQUENCE)
    if init_sequence is None:
        init_sequence = model.get_init_sequence(config)
    init_sequence = flatten_sequence(init_sequence)
    init_sequence_length = len(init_sequence)
    init_sequence_id = cg.static_const_array(
        config[CONF_INIT_SEQUENCE_ID], init_sequence
    )
    width, height = model.get_dimensions(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        model.name,
        width,
        height,
        init_sequence_id,
        init_sequence_length,
    )

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
