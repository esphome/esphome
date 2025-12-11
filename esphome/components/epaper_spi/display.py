import importlib
import pkgutil

from esphome import core, pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.display import CONF_SHOW_TEST_CARD, validate_rotation
from esphome.components.mipi import flatten_sequence, map_sequence
import esphome.config_validation as cv
from esphome.config_validation import update_interval
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_ENABLE_PIN,
    CONF_FULL_UPDATE_EVERY,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_DURATION,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
)
from esphome.cpp_generator import RawExpression
from esphome.final_validate import full_config

from . import models

AUTO_LOAD = ["split_buffer"]
DEPENDENCIES = ["spi"]

CONF_INIT_SEQUENCE_ID = "init_sequence_id"
CONF_MINIMUM_UPDATE_INTERVAL = "minimum_update_interval"

epaper_spi_ns = cg.esphome_ns.namespace("epaper_spi")
EPaperBase = epaper_spi_ns.class_(
    "EPaperBase", cg.PollingComponent, spi.SPIDevice, display.Display
)
Transform = epaper_spi_ns.enum("Transform")

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

TRANSFORM_OPTIONS = {CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY}


def model_schema(config):
    model = MODELS[config[CONF_MODEL]]
    class_name = epaper_spi_ns.class_(model.class_name, EPaperBase)
    minimum_update_interval = update_interval(
        model.get_default(CONF_MINIMUM_UPDATE_INTERVAL, "1s")
    )
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
                cv.Optional(CONF_ROTATION, default=0): validate_rotation,
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                cv.Optional(CONF_UPDATE_INTERVAL, default=cv.UNDEFINED): cv.All(
                    update_interval, cv.Range(min=minimum_update_interval)
                ),
                cv.Optional(CONF_TRANSFORM): cv.Schema(
                    {
                        cv.Required(CONF_MIRROR_X): cv.boolean,
                        cv.Required(CONF_MIRROR_Y): cv.boolean,
                    }
                ),
                cv.Optional(CONF_FULL_UPDATE_EVERY, default=1): cv.int_range(1, 255),
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
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True, space="-"),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    return model_schema(config)(config)


CONFIG_SCHEMA = customise_schema


def _final_validate(config):
    spi.final_validate_device_schema(
        "epaper_spi", require_miso=False, require_mosi=True
    )(config)

    global_config = full_config.get()
    from esphome.components.lvgl import DOMAIN as LVGL_DOMAIN

    if CONF_LAMBDA not in config and CONF_PAGES not in config:
        if LVGL_DOMAIN in global_config:
            if CONF_UPDATE_INTERVAL not in config:
                config[CONF_UPDATE_INTERVAL] = update_interval("never")
        else:
            # If no drawing methods are configured, and LVGL is not enabled, show a test card
            config[CONF_SHOW_TEST_CARD] = True
    elif CONF_UPDATE_INTERVAL not in config:
        config[CONF_UPDATE_INTERVAL] = update_interval("1min")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


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

    # Rotation is handled by setting the transform
    display_config = {k: v for k, v in config.items() if k != CONF_ROTATION}
    await display.register_display(var, display_config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    if reset_pin := config.get(CONF_RESET_PIN):
        reset = await cg.gpio_pin_expression(reset_pin)
        cg.add(var.set_reset_pin(reset))
    if busy_pin := config.get(CONF_BUSY_PIN):
        busy = await cg.gpio_pin_expression(busy_pin)
        cg.add(var.set_busy_pin(busy))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
    if CONF_RESET_DURATION in config:
        cg.add(var.set_reset_duration(config[CONF_RESET_DURATION]))
    if transform := config.get(CONF_TRANSFORM):
        transform[CONF_SWAP_XY] = False
    else:
        transform = {x: model.get_default(x, False) for x in TRANSFORM_OPTIONS}
    rotation = config[CONF_ROTATION]
    if rotation == 180:
        transform[CONF_MIRROR_X] = not transform[CONF_MIRROR_X]
        transform[CONF_MIRROR_Y] = not transform[CONF_MIRROR_Y]
    elif rotation == 90:
        transform[CONF_SWAP_XY] = not transform[CONF_SWAP_XY]
        transform[CONF_MIRROR_X] = not transform[CONF_MIRROR_X]
    elif rotation == 270:
        transform[CONF_SWAP_XY] = not transform[CONF_SWAP_XY]
        transform[CONF_MIRROR_Y] = not transform[CONF_MIRROR_Y]
    transform_str = "|".join(
        {
            str(getattr(Transform, x.upper()))
            for x in TRANSFORM_OPTIONS
            if transform.get(x)
        }
    )
    if transform_str:
        cg.add(var.set_transform(RawExpression(transform_str)))
