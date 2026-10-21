from typing import Any

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, output
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PHASE_ANGLE,
    CONF_PIN,
)
from esphome.types import ConfigType

DEPENDENCIES = ["esp32"]

CONF_DITHERING = "dithering"


def calc_max_frequency(bit_depth: int) -> float:
    return 80e6 / (2**bit_depth)


def calc_min_frequency(bit_depth: int) -> float:
    max_div_num = ((2**20) - 1) / 256.0
    return 80e6 / (max_div_num * (2**bit_depth))


def validate_frequency(value: Any) -> float:
    value = cv.frequency(value)
    min_freq = calc_min_frequency(20)
    max_freq = calc_max_frequency(1)
    if value < min_freq:
        raise cv.Invalid(
            f"This frequency setting is not possible, please choose a higher frequency (at least {int(min_freq)}Hz)"
        )
    if value > max_freq:
        raise cv.Invalid(
            f"This frequency setting is not possible, please choose a lower frequency (at most {int(max_freq)}Hz)"
        )
    return value


ledc_ns = cg.esphome_ns.namespace("ledc")
LEDCOutput = ledc_ns.class_("LEDCOutput", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(LEDCOutput),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): cv.All(
            cv.frequency, cv.float_range(min=0, min_included=False)
        ),
        cv.Optional(CONF_CHANNEL): cv.int_range(min=0, max=15),
        cv.Optional(CONF_DITHERING): cv.All(
            cv.boolean,
            esp32.only_on_variant(
                supported=[
                    esp32.VARIANT_ESP32,
                    esp32.VARIANT_ESP32C2,
                    esp32.VARIANT_ESP32C3,  # confirmed on hardware
                    esp32.VARIANT_ESP32C5,
                    esp32.VARIANT_ESP32C6,
                    esp32.VARIANT_ESP32C61,
                    esp32.VARIANT_ESP32H2,
                    esp32.VARIANT_ESP32P4,
                    esp32.VARIANT_ESP32S2,
                    esp32.VARIANT_ESP32S3,
                    esp32.VARIANT_ESP32S31,
                ],
                msg_prefix="LEDC hardware dithering",
            ),
        ),
        cv.Optional(CONF_PHASE_ANGLE): cv.All(
            cv.angle, cv.float_range(min=0.0, max=360.0)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # Re-enable the LEDC driver (excluded by default to save compile time)
    include_builtin_idf_component("esp_driver_ledc")

    gpio = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add_optional(var.set_channel, config.get(CONF_CHANNEL))
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))
    cg.add_optional(var.set_phase_angle, config.get(CONF_PHASE_ANGLE))
    if config.get(CONF_DITHERING):
        cg.add_define("USE_LEDC_DITHERING")
        cg.add(var.set_dithering(True))


automation.register_apply_action(
    "output.ledc.set_frequency",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LEDCOutput),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
    automation.ApplyField(CONF_FREQUENCY, "update_frequency", cg.float_),
)
