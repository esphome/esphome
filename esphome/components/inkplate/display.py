import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, i2c
from esphome.components.display import validate_rotation
from esphome.components.esp32 import CONF_CPU_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_FULL_UPDATE_EVERY,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_ROTATION,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
    PLATFORM_ESP32,
)
import esphome.final_validate as fv

from .models import InkplateModel as InkplateModelDef

DEPENDENCIES = ["i2c", "esp32", "psram"]

DOMAIN = "inkplate"

# Pin name lists
REQUIRED_CONTROL_PINS = [
    pin + "_pin"
    for pin in (
        "ckv",
        "gmod",
        "gpio0_enable",
        "oe",
        "powerup",
        "sph",
        "spv",
        "vcom",
        "wakeup",
    )
]
OPTIONAL_CONTROL_PINS = [pin + "_pin" for pin in ("cl", "le")]

# Generate config keys
CONF_DISPLAY_DATA_PINS = "display_data_pins"

# Generate list of individual data pin config keys
DATA_PIN_KEYS = (f"display_data_{i}_pin" for i in range(8))

# Other config keys
CONF_GREYSCALE = "greyscale"
CONF_PARTIAL_UPDATING = "partial_updating"

inkplate_ns = cg.esphome_ns.namespace("inkplate")
Inkplate = inkplate_ns.class_(
    "Inkplate",
    cg.PollingComponent,
    i2c.I2CDevice,
    display.Display,
    display.DisplayBuffer,
)

InkplateModelEnum = inkplate_ns.enum("InkplateModel")
Transform = inkplate_ns.enum("Transform")

# Generate MODELS dictionary from InkplateModelDef
MODELS = InkplateModelDef.get_models()

CONF_CUSTOM_WAVEFORM = "custom_waveform"
CONF_CUSTOM_WAVEFORMS = "custom_waveforms"

DIMENSION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WIDTH): cv.int_,
        cv.Required(CONF_HEIGHT): cv.int_,
    }
)

TRANSFORM_OPTIONS = {CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY}

_LOGGER = logging.getLogger(DOMAIN)


def _validate_data_pins(config):
    # Check if both old and new formats are used
    old_format_used = any(key in config for key in DATA_PIN_KEYS)
    new_format_used = CONF_DISPLAY_DATA_PINS in config

    if old_format_used and new_format_used:
        raise cv.Invalid(
            f"Cannot use both '{CONF_DISPLAY_DATA_PINS}' and individual data pin configurations. "
            "Use only the new 'display_data_pins' array format.",
            path=[CONF_DISPLAY_DATA_PINS],
        )

    if old_format_used:
        _LOGGER.warning(
            "Individual data pin configuration (display_data_0_pin through display_data_7_pin) is deprecated. "
            f"Use '{CONF_DISPLAY_DATA_PINS}' with an array of 8 pins instead."
        )

    if old_format_used and not new_format_used:
        if any(pin not in config for pin in DATA_PIN_KEYS):
            raise cv.Invalid(
                "All individual data pin configurations (display_data_0_pin through display_data_7_pin) must be specified."
            )
        config[CONF_DISPLAY_DATA_PINS] = [config[pin] for pin in DATA_PIN_KEYS]

    return config


# Build schema dict with dynamically generated data pin entries
def _model_schema(config):
    model = MODELS[config[CONF_MODEL]]
    cv_dimensions = cv.Optional if model.get_default(CONF_WIDTH) else cv.Required
    custom_waveforms = model.get_default(CONF_CUSTOM_WAVEFORMS)
    valid_waveform = (
        cv.All(cv.uint8_t, cv.Range(min=1, max=len(custom_waveforms)))
        if custom_waveforms
        else cv.invalid("Custom waveforms are not supported on this model")
    )
    return (
        display.FULL_DISPLAY_SCHEMA.extend(cv.polling_component_schema("5s"))
        .extend(i2c.i2c_device_schema(0x48))
        .extend(
            {
                **{
                    model.option(pin, fallback=None): pins.gpio_output_pin_schema
                    for pin in REQUIRED_CONTROL_PINS
                },
                **{
                    model.option(pin): pins.gpio_output_pin_schema
                    for pin in OPTIONAL_CONTROL_PINS
                },
                cv.Optional(CONF_CUSTOM_WAVEFORM): valid_waveform,
                cv.Optional(CONF_ROTATION, default=0): validate_rotation,
                cv.Required(CONF_MODEL): cv.one_of(model.name, upper=True),
                cv.Optional(
                    CONF_UPDATE_INTERVAL, default=cv.UNDEFINED
                ): cv.update_interval,
                cv.Optional(CONF_TRANSFORM): cv.Schema(
                    {
                        cv.Required(CONF_MIRROR_X): cv.boolean,
                        cv.Required(CONF_MIRROR_Y): cv.boolean,
                    }
                ),
                cv.Optional(CONF_FULL_UPDATE_EVERY, default=1): cv.int_range(1, 255),
                cv.GenerateID(): cv.declare_id(Inkplate),
                cv_dimensions(CONF_DIMENSIONS): DIMENSION_SCHEMA,
                cv.Optional(CONF_GREYSCALE, default=False): cv.boolean,
                cv.Optional(CONF_PARTIAL_UPDATING, default=True): cv.boolean,
                cv.Optional(CONF_FULL_UPDATE_EVERY, default=1): cv.uint32_t,
                model.option(CONF_DISPLAY_DATA_PINS): cv.All(
                    [pins.internal_gpio_output_pin_schema],
                    cv.Length(min=8, max=8),
                ),
                **{
                    cv.Optional(pin_key): pins.internal_gpio_output_pin_schema
                    for pin_key in DATA_PIN_KEYS
                },
            }
        )
    )


def _customise_schema(config):
    """
    Create a customised config schema for a specific model and validate the configuration.
    """
    config = cv.Schema(
        {
            cv.Required(CONF_MODEL): cv.one_of(*MODELS, upper=True, space="-"),
        },
        extra=cv.ALLOW_EXTRA,
    )(config)
    config = _model_schema(config)(config)
    return _validate_data_pins(config)


CONFIG_SCHEMA = _customise_schema


def _validate_cpu_frequency(config):
    esp32_config = fv.full_config.get()[PLATFORM_ESP32]
    if esp32_config[CONF_CPU_FREQUENCY] != "240MHZ":
        raise cv.Invalid(
            "Inkplate requires 240MHz CPU frequency (set in esp32 component)"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_cpu_frequency


async def to_code(config):
    # Get the model definition
    model = InkplateModelDef.models[config[CONF_MODEL]]

    # Build all GPIO pin expressions
    required_control_pins = [
        await cg.gpio_pin_expression(config[pin]) for pin in REQUIRED_CONTROL_PINS
    ]
    optional_control_pins = [
        await cg.gpio_pin_expression(config[pin]) if pin in config else cg.nullptr
        for pin in OPTIONAL_CONTROL_PINS
    ]
    # Data pins (already normalized in validation)
    data_pins_expr = [
        await cg.gpio_pin_expression(pin_cfg)
        for pin_cfg in config[CONF_DISPLAY_DATA_PINS]
    ]

    width, height = model.get_dimensions(config)
    args = [
        InkplateModelEnum.__getattr__(config[CONF_MODEL]),
        width,
        height,
        data_pins_expr,
        *required_control_pins,
        *optional_control_pins,
    ]
    # Construct Inkplate with pins
    var = cg.new_Pvariable(config[CONF_ID], *args)

    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_greyscale(config[CONF_GREYSCALE]))
    cg.add(var.set_partial_updating(config[CONF_PARTIAL_UPDATING]))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))

    if custom_waveform := config.get(CONF_CUSTOM_WAVEFORM):
        # Use custom waveform for Inkplate 10
        waveform = model.get_default(CONF_CUSTOM_WAVEFORMS)[custom_waveform - 1]
        waveform = [element for tupl in waveform for element in tupl]
        cg.add(var.set_waveform(waveform, True))
    else:
        # Use the model's default waveform
        waveform = [element for tupl in model.waveform for element in tupl]
        cg.add(var.set_waveform(waveform, False))

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
        cg.add(var.set_transform(cg.RawExpression(transform_str)))
