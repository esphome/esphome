from esphome import pins
import esphome.codegen as cg
from esphome.components import display, i2c
from esphome.components.esp32 import CONF_CPU_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_FULL_UPDATE_EVERY,
    CONF_ID,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_NUMBER,
    CONF_OE_PIN,
    CONF_PAGES,
    CONF_TRANSFORM,
    CONF_WAKEUP_PIN,
    PLATFORM_ESP32,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .const import INKPLATE_10_CUSTOM_WAVEFORMS, WAVEFORMS

DEPENDENCIES = ["i2c", "esp32", "psram"]

CONF_DISPLAY_DATA_0_PIN = "display_data_0_pin"
CONF_DISPLAY_DATA_1_PIN = "display_data_1_pin"
CONF_DISPLAY_DATA_2_PIN = "display_data_2_pin"
CONF_DISPLAY_DATA_3_PIN = "display_data_3_pin"
CONF_DISPLAY_DATA_4_PIN = "display_data_4_pin"
CONF_DISPLAY_DATA_5_PIN = "display_data_5_pin"
CONF_DISPLAY_DATA_6_PIN = "display_data_6_pin"
CONF_DISPLAY_DATA_7_PIN = "display_data_7_pin"

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

InkplateModel = inkplate_ns.enum("InkplateModel")

MODELS = InkplateModel.options(
    "inkplate_6",
    "inkplate_10",
    "inkplate_6_plus",
    "inkplate_6_v2",
    "inkplate_5",
    "inkplate_5_v2",
)

CONF_CUSTOM_WAVEFORM = "custom_waveform"


def _validate_custom_waveform(config: ConfigType) -> ConfigType:
    if CONF_CUSTOM_WAVEFORM in config and config[CONF_MODEL] != "inkplate_10":
        raise cv.Invalid("Custom waveforms are only supported on the Inkplate 10")
    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
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
            cv.Optional(CONF_FULL_UPDATE_EVERY, default=10): cv.uint32_t,
            cv.Optional(CONF_MODEL, default="inkplate_6"): cv.enum(
                MODELS, lower=True, space="_"
            ),
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
            cv.Optional(
                CONF_CL_PIN,
                default={CONF_NUMBER: 0, CONF_IGNORE_STRAPPING_WARNING: True},
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_LE_PIN,
                default={CONF_NUMBER: 2, CONF_IGNORE_STRAPPING_WARNING: True},
            ): pins.internal_gpio_output_pin_schema,
            # Data pins
            cv.Optional(
                CONF_DISPLAY_DATA_0_PIN, default=4
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_1_PIN,
                default={CONF_NUMBER: 5, CONF_IGNORE_STRAPPING_WARNING: True},
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_2_PIN, default=18
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_3_PIN, default=19
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_4_PIN, default=23
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_5_PIN, default=25
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_6_PIN, default=26
            ): pins.internal_gpio_output_pin_schema,
            cv.Optional(
                CONF_DISPLAY_DATA_7_PIN, default=27
            ): pins.internal_gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x48)),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
    _validate_custom_waveform,
)


def _validate_cpu_frequency(config: ConfigType) -> None:
    esp32_config = fv.full_config.get()[PLATFORM_ESP32]
    if esp32_config[CONF_CPU_FREQUENCY] != "240MHZ":
        raise cv.Invalid(
            "Inkplate requires 240MHz CPU frequency (set in esp32 component)"
        )


FINAL_VALIDATE_SCHEMA = _validate_cpu_frequency


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])

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
        waveform = INKPLATE_10_CUSTOM_WAVEFORMS[custom_waveform - 1]
        waveform = [element for tupl in waveform for element in tupl]
        cg.add(var.set_waveform(waveform, True))
    else:
        waveform = WAVEFORMS[config[CONF_MODEL]]
        waveform = [element for tupl in waveform for element in tupl]
        cg.add(var.set_waveform(waveform, False))

    await cg.add_pin(var.set_ckv_pin, config[CONF_CKV_PIN])
    await cg.add_pin(var.set_gmod_pin, config[CONF_GMOD_PIN])
    await cg.add_pin(var.set_gpio0_enable_pin, config[CONF_GPIO0_ENABLE_PIN])
    await cg.add_pin(var.set_oe_pin, config[CONF_OE_PIN])
    await cg.add_pin(var.set_powerup_pin, config[CONF_POWERUP_PIN])
    await cg.add_pin(var.set_sph_pin, config[CONF_SPH_PIN])
    await cg.add_pin(var.set_spv_pin, config[CONF_SPV_PIN])
    await cg.add_pin(var.set_vcom_pin, config[CONF_VCOM_PIN])
    await cg.add_pin(var.set_wakeup_pin, config[CONF_WAKEUP_PIN])
    await cg.add_pin(var.set_cl_pin, config[CONF_CL_PIN])
    await cg.add_pin(var.set_le_pin, config[CONF_LE_PIN])
    await cg.add_pin(var.set_display_data_0_pin, config[CONF_DISPLAY_DATA_0_PIN])
    await cg.add_pin(var.set_display_data_1_pin, config[CONF_DISPLAY_DATA_1_PIN])
    await cg.add_pin(var.set_display_data_2_pin, config[CONF_DISPLAY_DATA_2_PIN])
    await cg.add_pin(var.set_display_data_3_pin, config[CONF_DISPLAY_DATA_3_PIN])
    await cg.add_pin(var.set_display_data_4_pin, config[CONF_DISPLAY_DATA_4_PIN])
    await cg.add_pin(var.set_display_data_5_pin, config[CONF_DISPLAY_DATA_5_PIN])
    await cg.add_pin(var.set_display_data_6_pin, config[CONF_DISPLAY_DATA_6_PIN])
    await cg.add_pin(var.set_display_data_7_pin, config[CONF_DISPLAY_DATA_7_PIN])
