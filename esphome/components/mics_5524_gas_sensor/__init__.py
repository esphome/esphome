"""``mics_5524_gas_sensor`` - ESPHome platform for the MiCS-5524 gas sensor.

The MiCS-5524 is the small MEMS multi-gas sensor used as the trace / early-warning
device of this project (100 - 1000 ppm of H2, i.e. 0.25 - 2.5 % of the LEL), where
the MQ-8 covers the 4000 - 10000 ppm pre-alarm band.

Two conversion models are available (see ``coefficients.py``):

``conversion: dfrobot`` (default)
    The vendor model of the DFRobot_MICS library (MIT) used by the analog
    breakout: ``ratio = (VCC - V_AO) / (VCC - V_AO_air)`` and
    ``ppm = (threshold - ratio) / gain`` per gas. The reference value is captured
    in clean air, so the board's load resistor and divider do not matter.

``conversion: datasheet``
    ``RS = (VCC * RL) / V_AO - RL``, ``ppm = a * (RS / R0)^b`` - the classic
    datasheet fit, only shipped for CO (``a = 6.3``, ``b = -1.1``).

The analog signal comes either from an existing voltage sampler (``voltage:`` -
an ``adc`` or ``ads1115`` sensor) or from a pin this component owns (``pin:``,
which creates a hidden internal ``adc`` sensor for you).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
AUTO_LOAD = ["sensor", "voltage_sampler"]

mics_5524_gas_sensor_ns = cg.esphome_ns.namespace("mics_5524_gas_sensor")

# C++ class: class MiCS5524GasSensor : public sensor::Sensor, public PollingComponent
MiCS5524GasSensor = mics_5524_gas_sensor_ns.class_(
    "MiCS5524GasSensor", sensor.Sensor, cg.PollingComponent
)

# ---------------------------------------------------------------------------
# Configuration keys
# ---------------------------------------------------------------------------
# The keys shared with other components (`warmup_time`, `enable_pin`) are
# imported from esphome.const by the platform schema.
CONF_GAS = "gas"
CONF_CONVERSION = "conversion"
CONF_A = "a"
CONF_B = "b"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_DIVIDER = "divider"
CONF_R1 = "r1"
CONF_R2 = "r2"
CONF_ADC_INPUT_MAX = "adc_input_max"
CONF_ADC_PIN_MAX = "adc_pin_max"
CONF_ADC_ATTENUATION = "adc_attenuation"
CONF_ADC_SAMPLES = "adc_samples"
CONF_VCC = "vcc"
CONF_RL = "rl"
CONF_R0 = "r0"
CONF_AIR_REFERENCE = "air_reference"
CONF_SAMPLES = "samples"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"
CONF_LOG_SENSOR = "log_sensor"
CONF_LOG_PPM = "log_ppm"
CONF_PERSIST = "persist"
CONF_RATIO_SENSOR = "ratio_sensor"
CONF_RS_SENSOR = "rs_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"

# Conversion models, mirroring micsmath::ConversionModel.
CONVERSION_MODELS = {
    "dfrobot": 0,  # DFRobot_MICS vendor model (analog breakout)
    "datasheet": 1,  # RS/R0 power law fitted on the datasheet curve
}

# Default ADC input limits for the classic ESP32, used by the divider guard:
# the recommended maximum input voltage (VDD) and the datasheet's absolute
# maximum (VDD + 0.3 V).  Override them per sensor for other samplers, e.g.
# `adc_input_max: 6.144` / `adc_pin_max: 6.144` for an ADS1115 at the widest gain.
ESP32_ADC_INPUT_MAX_V = 3.3
ESP32_ADC_PIN_MAX_V = 3.6

#: Comparison margin of the divider guard (V): a declared limit is honoured even
#: when the divider arithmetic lands a few millivolts above it, so that
#: `adc_input_max: 3.33` accepts the 5 V * (20/30) = 3.3333 V of a 10k/20k divider.
ADC_LIMIT_MARGIN_V = 0.01
