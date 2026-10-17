"""MiCS-5524 gas sensor platform with the DFRobot vendor model and the datasheet fit."""

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
from esphome.const import PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mics_5524"

mics_5524_ns = cg.esphome_ns.namespace("mics_5524")
micsmath_ns = mics_5524_ns.namespace("micsmath")
ConversionModel = micsmath_ns.enum("ConversionModel")

MiCS5524Component = mics_5524_ns.class_("MiCS5524Component", cg.PollingComponent)
MiCS5524GasSensor = mics_5524_ns.class_("MiCS5524GasSensor", sensor.Sensor)

CONF_CONVERSION = "conversion"
CONF_COEFFICIENT_A = "coefficient_a"
CONF_COEFFICIENT_B = "coefficient_b"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_ADC_INPUT_MAX = "adc_input_max"
CONF_ADC_PIN_MAX = "adc_pin_max"
CONF_ADC_ATTENUATION = "adc_attenuation"
CONF_R1 = "r1"
CONF_R2 = "r2"
CONF_VCC = "vcc"
CONF_RL = "rl"
CONF_R0 = "r0"
CONF_AIR_REFERENCE = "air_reference"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_PERSIST = "persist"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"
CONF_RATIO_SENSOR = "ratio_sensor"
CONF_RS_SENSOR = "rs_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"

CONVERSION_MODELS = {
    "dfrobot": ConversionModel.CONVERSION_MODEL_DFROBOT,
    "datasheet": ConversionModel.CONVERSION_MODEL_DATASHEET,
}

# Divider guard limits (input max, pin max) in volts by target platform. ESP32 and RP2: VDD and
# the absolute maximum VDD + 0.3 V; a bare ESP8266 A0 measures 0 - 1 V, boards with an onboard
# divider declare more. Any other platform declares both limits.
ADC_LIMITS_V = {
    PLATFORM_ESP32: (3.3, 3.6),
    PLATFORM_ESP8266: (1.0, 1.0),
    PLATFORM_RP2: (3.3, 3.6),
}

# Top of the recommended input range of each ESP32 ADC attenuation, in volts, from the ESP-IDF ADC
# calibration documentation; above it the reading flattens. The ESP32 row is the most conservative
# and stands in for variants without their own row. "auto" ends up at the widest range.
_ESP32_ATTENUATION_INPUT_MAX_V = {
    VARIANT_ESP32: {"0db": 0.95, "2.5db": 1.25, "6db": 1.75, "12db": 2.45},
    VARIANT_ESP32S2: {"0db": 0.75, "2.5db": 1.05, "6db": 1.3, "12db": 2.5},
    VARIANT_ESP32S3: {"0db": 0.95, "2.5db": 1.25, "6db": 1.75, "12db": 3.1},
    VARIANT_ESP32C3: {"0db": 0.75, "2.5db": 1.05, "6db": 1.3, "12db": 2.5},
    VARIANT_ESP32C6: {"0db": 0.95, "2.5db": 1.25, "6db": 1.75, "12db": 3.3},
}


def esp32_attenuation_input_max(variant: str, attenuation: str) -> float:
    """Largest input the internal ADC measures at the attenuation, for the variant."""
    table = _ESP32_ATTENUATION_INPUT_MAX_V.get(
        variant, _ESP32_ATTENUATION_INPUT_MAX_V[VARIANT_ESP32]
    )
    if attenuation in ("11db", "auto"):
        attenuation = "12db"
    return table[attenuation]


# Guard margin (V): the 3.3333 V of a 10k/20k divider still passes adc_input_max: 3.33.
ADC_LIMIT_MARGIN_V = 0.01
