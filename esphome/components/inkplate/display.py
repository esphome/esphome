import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import display, i2c
from esphome.components.esp32 import CONF_CPU_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OE_PIN,
    CONF_PAGES,
    CONF_TRANSFORM,
    CONF_WAKEUP_PIN,
    PLATFORM_ESP32,
)
import esphome.final_validate as fv

from .const import INKPLATE_10_CUSTOM_WAVEFORMS
from .models import InkplateModel as InkplateModelDef

DEPENDENCIES = ["i2c", "esp32", "psram"]

DOMAIN = "inkplate"

CONF_DISPLAY_DATA_PINS = "display_data_pins"

# Generate list of individual data pin config keys
DATA_PIN_KEYS = [f"display_data_{i}_pin" for i in range(8)]

CONF_CL_PIN = "cl_pin"
CONF_CKV_PIN = "ckv_pin"
CONF_GREYSCALE = "greyscale"
CONF_GMOD_PIN = "gmod_pin"
CONF_GPIO0_ENABLE_PIN = "gpio0_enable_pin"
CONF_LE_PIN = "le_pin"
CONF_PARTIAL_UPDATING = "partial_updating"
CONF_POWERUP_PIN = "powerup_pin"
CONF_SPH_PIN = "sph_pin"
CONF_SPV_PIN = "spv_pin"
CONF_VCOM_PIN = "vcom_pin"

inkplate_ns = cg.esphome_ns.namespace("inkplate")
Inkplate = inkplate_ns.class_(
    "Inkplate",
    cg.PollingComponent,
    i2c.I2CDevice,
    display.Display,
    display.DisplayBuffer,
)

InkplateModelEnum = inkplate_ns.enum("InkplateModel")

# Generate MODELS dictionary from InkplateModelDef
MODELS = {
    name: getattr(InkplateModelEnum, enum)
    for name, enum in InkplateModelDef.get_models().items()
}

CONF_CUSTOM_WAVEFORM = "custom_waveform"


def _validate_custom_waveform(config):
    if CONF_CUSTOM_WAVEFORM in config and config[CONF_MODEL] != "inkplate_10":
        raise cv.Invalid("Custom waveforms are only supported on the Inkplate 10")
    return config


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

    # Apply model defaults if no data pins are specified
    if not new_format_used and not old_format_used:
        # Get model defaults
        model_def = InkplateModelDef.models[config[CONF_MODEL]]
        # Validate and normalize each default pin
        config[CONF_DISPLAY_DATA_PINS] = [
            pins.internal_gpio_output_pin_schema(pin) for pin in model_def.data_pins
        ]
    elif old_format_used and not new_format_used:
        if any(pin not in config for pin in DATA_PIN_KEYS):
            raise cv.Invalid(
                "All individual data pin configurations (display_data_0_pin through display_data_7_pin) must be specified."
            )
        config[CONF_DISPLAY_DATA_PINS] = [config[pin] for pin in DATA_PIN_KEYS]

    # Also apply defaults for CL and LE pins if not specified
    model_def = InkplateModelDef.models[config[CONF_MODEL]]
    if CONF_CL_PIN not in config:
        config[CONF_CL_PIN] = pins.internal_gpio_output_pin_schema(model_def.cl_pin)
    if CONF_LE_PIN not in config:
        config[CONF_LE_PIN] = pins.internal_gpio_output_pin_schema(model_def.le_pin)

    return config


# Build schema dict with dynamically generated data pin entries
_SCHEMA_DICT = {
    cv.GenerateID(): cv.declare_id(Inkplate),
    cv.Optional(CONF_GREYSCALE, default=False): cv.boolean,
    cv.Optional(CONF_CUSTOM_WAVEFORM): cv.All(
        cv.uint8_t, cv.Range(min=1, max=len(INKPLATE_10_CUSTOM_WAVEFORMS))
    ),
    cv.Optional(CONF_TRANSFORM): cv.Schema(
        {
            cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
            cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
        }
    ),
    cv.Optional(CONF_PARTIAL_UPDATING, default=True): cv.boolean,
    cv.Optional(CONF_FULL_UPDATE_EVERY, default=1): cv.uint32_t,
    cv.Required(CONF_MODEL): cv.enum(MODELS, lower=True, space="_"),
    # Control pins
    cv.Required(CONF_CKV_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_GMOD_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_GPIO0_ENABLE_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_OE_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_POWERUP_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_SPH_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_SPV_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_VCOM_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_WAKEUP_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_CL_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_LE_PIN): pins.internal_gpio_output_pin_schema,
    # Data pins - new array format
    cv.Optional(CONF_DISPLAY_DATA_PINS): cv.All(
        [pins.internal_gpio_output_pin_schema],
        cv.Length(min=8, max=8),
    ),
    **{
        cv.Optional(pin_key): pins.internal_gpio_output_pin_schema
        for pin_key in DATA_PIN_KEYS
    },
}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(_SCHEMA_DICT)
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x48)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate_custom_waveform,
    _validate_data_pins,
)


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
    model_def = InkplateModelDef.models[config[CONF_MODEL]]

    # Build all GPIO pin expressions
    ckv = await cg.gpio_pin_expression(config[CONF_CKV_PIN])
    gmod = await cg.gpio_pin_expression(config[CONF_GMOD_PIN])
    gpio0_enable = await cg.gpio_pin_expression(config[CONF_GPIO0_ENABLE_PIN])
    oe = await cg.gpio_pin_expression(config[CONF_OE_PIN])
    powerup = await cg.gpio_pin_expression(config[CONF_POWERUP_PIN])
    sph = await cg.gpio_pin_expression(config[CONF_SPH_PIN])
    spv = await cg.gpio_pin_expression(config[CONF_SPV_PIN])
    vcom = await cg.gpio_pin_expression(config[CONF_VCOM_PIN])
    wakeup = await cg.gpio_pin_expression(config[CONF_WAKEUP_PIN])
    cl = await cg.gpio_pin_expression(config[CONF_CL_PIN])
    le = await cg.gpio_pin_expression(config[CONF_LE_PIN])

    # Data pins (already normalized in validation)
    data_pins_expr = [
        await cg.gpio_pin_expression(pin_cfg)
        for pin_cfg in config[CONF_DISPLAY_DATA_PINS]
    ]

    # Construct Inkplate with pins
    var = cg.new_Pvariable(
        config[CONF_ID],
        data_pins_expr,
        ckv,
        cl,
        gpio0_enable,
        gmod,
        le,
        oe,
        powerup,
        sph,
        spv,
        vcom,
        wakeup,
    )

    await display.register_display(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_greyscale(config[CONF_GREYSCALE]))
    if transform := config.get(CONF_TRANSFORM):
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))
    cg.add(var.set_partial_updating(config[CONF_PARTIAL_UPDATING]))
    cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))

    cg.add(var.set_model(config[CONF_MODEL]))

    if custom_waveform := config.get(CONF_CUSTOM_WAVEFORM):
        # Use custom waveform for Inkplate 10
        waveform = model_def.custom_waveforms[custom_waveform - 1]
        waveform = [element for tupl in waveform for element in tupl]
        cg.add(var.set_waveform(waveform, True))
    else:
        # Use the model's default waveform
        waveform = [element for tupl in model_def.waveform for element in tupl]
        cg.add(var.set_waveform(waveform, False))
