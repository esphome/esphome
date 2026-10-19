import importlib
import pkgutil

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, spi
from esphome.components.display import CONF_SHOW_TEST_CARD
from esphome.components.mipi import (
    DriverChip,
    flatten_sequence,
    map_sequence,
    model_schema_extractor,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_CS_PIN,
    CONF_DATA_RATE,
    CONF_DC_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INIT_SEQUENCE,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_RESET_PIN,
    CONF_ROTATION,
    CONF_WIDTH,
)
from esphome.types import ConfigType

from . import models

DEPENDENCIES = ["spi"]

CONF_COLUMN_START = "column_start"
CONF_COLUMN_END = "column_end"
CONF_PORTRAIT = "portrait"
CONF_ROW_START = "row_start"
CONF_ROW_END = "row_end"

st7305_ns = cg.esphome_ns.namespace("st7305")
ST7305 = st7305_ns.class_("ST7305", display.DisplayBuffer, spi.SPIDevice)

Orientation = st7305_ns.enum("Orientation")

# CUSTOM must be registered before the model modules are imported.
DriverChip("CUSTOM")
for module_info in pkgutil.iter_modules(models.__path__):
    importlib.import_module(f".models.{module_info.name}", package=__package__)
MODELS = DriverChip.get_models()

DEFAULT_MODEL = "WAVESHARE-400X300"


def _model_schema(model: DriverChip) -> cv.Schema:
    iseqconf = (
        cv.Required(CONF_INIT_SEQUENCE)
        if model.initsequence is None
        else cv.Optional(CONF_INIT_SEQUENCE)
    )
    return (
        display.FULL_DISPLAY_SCHEMA.extend(
            spi.spi_device_schema(
                cs_pin_required=False,
                default_data_rate=model.get_default(CONF_DATA_RATE, 10_000_000),
                default_mode="MODE0",
            )
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(
            {
                cv.GenerateID(): cv.declare_id(ST7305),
                cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True),
                model.option(CONF_CS_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
                model.option(CONF_DC_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
                model.option(CONF_RESET_PIN, cv.UNDEFINED): pins.gpio_output_pin_schema,
                cv.Optional(CONF_WIDTH): cv.int_range(min=8, max=1024),
                cv.Optional(CONF_HEIGHT): cv.int_range(min=8, max=1024),
                cv.Optional(CONF_PORTRAIT): cv.boolean,
                cv.Optional(CONF_COLUMN_START): cv.int_range(min=0, max=255),
                cv.Optional(CONF_COLUMN_END): cv.int_range(min=0, max=255),
                cv.Optional(CONF_ROW_START): cv.int_range(min=0, max=255),
                cv.Optional(CONF_ROW_END): cv.int_range(min=0, max=255),
                iseqconf: cv.ensure_list(map_sequence),
            }
        )
    )


def _resolve(config: ConfigType, model: DriverChip) -> None:
    """Fill in model defaults and validate the geometry and address window."""
    if (width := config.get(CONF_WIDTH)) is None:
        if (width := model.get_default(CONF_WIDTH, None)) is None:
            raise cv.Invalid("'width' is required for this model", path=[CONF_WIDTH])
        config[CONF_WIDTH] = width
    if (height := config.get(CONF_HEIGHT)) is None:
        if (height := model.get_default(CONF_HEIGHT, None)) is None:
            raise cv.Invalid("'height' is required for this model", path=[CONF_HEIGHT])
        config[CONF_HEIGHT] = height

    if config.get(CONF_PORTRAIT) is None:
        if (default := model.get_default(CONF_PORTRAIT, None)) is None:
            raise cv.Invalid(
                "'portrait' is required for this model", path=[CONF_PORTRAIT]
            )
        config[CONF_PORTRAIT] = default

    if config[CONF_PORTRAIT]:
        if width % 4 or height % 2:
            raise cv.Invalid(
                "portrait orientation requires a width divisible by 4 and a height divisible by 2"
            )
        columns = (width + 11) // 12
        rows = (height + 1) // 2
    else:
        if width % 2 or height % 4:
            raise cv.Invalid(
                "landscape orientation requires a width divisible by 2 and a height divisible by 4"
            )
        columns = (height + 11) // 12
        rows = (width + 1) // 2

    column_start = config.get(
        CONF_COLUMN_START, model.get_default(CONF_COLUMN_START, 0)
    )
    row_start = config.get(CONF_ROW_START, model.get_default(CONF_ROW_START, 0))
    column_end = config.get(
        CONF_COLUMN_END, model.get_default(CONF_COLUMN_END, column_start + columns - 1)
    )
    row_end = config.get(
        CONF_ROW_END, model.get_default(CONF_ROW_END, row_start + rows - 1)
    )
    if column_end < column_start or row_end < row_start:
        raise cv.Invalid("address window end must not be before its start")
    if (column_end - column_start + 1) * (row_end - row_start + 1) * 3 < (
        width * height // 8
    ):
        raise cv.Invalid("address window is too small for the display dimensions")
    config[CONF_COLUMN_START] = column_start
    config[CONF_COLUMN_END] = column_end
    config[CONF_ROW_START] = row_start
    config[CONF_ROW_END] = row_end

    if config.get(CONF_INIT_SEQUENCE) is None:
        config[CONF_INIT_SEQUENCE] = model.initsequence

    for key, label in ((CONF_CS_PIN, "cs_pin"), (CONF_DC_PIN, "dc_pin")):
        if key not in config:
            raise cv.Invalid(f"'{label}' is required for this model", path=[key])


def model_schema(config: ConfigType) -> cv.Schema:
    """Return the schema for the model selected in the config."""
    return _model_schema(MODELS[config[CONF_MODEL]])


@model_schema_extractor(MODELS, model_schema)
def customise_schema(config: ConfigType) -> ConfigType:
    config = cv.Schema(
        {
            cv.Optional(CONF_MODEL, default=DEFAULT_MODEL): cv.one_of(
                *MODELS, upper=True
            )
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    model = MODELS[config[CONF_MODEL]]
    config = _model_schema(model)(config)
    _resolve(config, model)

    display.add_metadata(
        config[CONF_ID],
        config[CONF_WIDTH],
        config[CONF_HEIGHT],
        has_hardware_rotation=False,
        byte_order=cv.UNDEFINED,
        has_writer=config.get(CONF_AUTO_CLEAR_ENABLED) is True
        or config.get(CONF_PAGES) is not None
        or config.get(CONF_LAMBDA) is not None
        or config.get(CONF_SHOW_TEST_CARD) is True,
        rotation=config.get(CONF_ROTATION, 0),
    )
    return config


CONFIG_SCHEMA = customise_schema

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "st7305", require_miso=False, require_mosi=True
)


async def to_code(config: ConfigType) -> None:
    sequence = flatten_sequence(config[CONF_INIT_SEQUENCE])
    table = cg.shared_progmem_array("st7305_init_sequence", cg.uint8, sequence)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_MODEL],
        config[CONF_WIDTH],
        config[CONF_HEIGHT],
        Orientation.ORIENTATION_PORTRAIT
        if config[CONF_PORTRAIT]
        else Orientation.ORIENTATION_LANDSCAPE,
        config[CONF_COLUMN_START],
        config[CONF_COLUMN_END],
        config[CONF_ROW_START],
        config[CONF_ROW_END],
        table,
        len(sequence),
    )

    await display.register_display(var, config)
    await spi.register_spi_device(var, config, write_only=True)
    await cg.add_pin(var.set_dc_pin, config[CONF_DC_PIN])
    await cg.add_pin(var.set_reset_pin, config.get(CONF_RESET_PIN))
    await display.add_writer(var, config)
