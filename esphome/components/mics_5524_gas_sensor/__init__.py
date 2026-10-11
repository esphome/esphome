"""``mics_5524_gas_sensor`` - ESPHome platform for the MiCS-5524 gas sensor.

The conversion is the vendor model of the DFRobot_MICS library (MIT) used by the
analog breakout (constants and source in ``coefficients.py``):
``ratio = (VCC - V_AO) / (VCC - V_AO_air)``, ``ppm = (threshold - ratio) / gain``
per gas.  The reference value is captured in clean air, so the board's load
resistor and its divider do not matter.

The analog signal comes either from an existing voltage sampler (``voltage:``)
or from a pin this component owns (``pin:``, which creates an internal ``adc``
sensor for you).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mics_5524_gas_sensor"

# No ``AUTO_LOAD``: for a platform component ESPHome loads what ``sensor.py``
# declares (``sensor``, ``voltage_sampler``, and ``adc`` for the internal ADC
# sensor of a ``pin:`` configuration).

mics_5524_gas_sensor_ns = cg.esphome_ns.namespace("mics_5524_gas_sensor")

# C++ class: class MiCS5524GasSensor : public sensor::Sensor, public PollingComponent
MiCS5524GasSensor = mics_5524_gas_sensor_ns.class_(
    "MiCS5524GasSensor", sensor.Sensor, cg.PollingComponent
)

# Configuration keys local to this component; the keys shared with other
# components (`warmup_time`, `enable_pin`, `voltage_divider`) are imported from
# esphome.const by the platform schema.
CONF_GAS = "gas"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_R1 = "r1"
CONF_R2 = "r2"
CONF_ADC_INPUT_MAX = "adc_input_max"
CONF_ADC_PIN_MAX = "adc_pin_max"
CONF_ADC_ATTENUATION = "adc_attenuation"
CONF_ADC_SAMPLES = "adc_samples"
CONF_VCC = "vcc"
CONF_AIR_REFERENCE = "air_reference"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"
CONF_LOG_SENSOR = "log_sensor"
CONF_LOG_PPM = "log_ppm"
CONF_PERSIST = "persist"
CONF_RATIO_SENSOR = "ratio_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"

# Divider guard limits of the classic ESP32: the recommended maximum input
# voltage (VDD) and the datasheet's absolute maximum (VDD + 0.3 V).  Override them
# per sensor for other samplers, e.g. `adc_input_max: 6.144` for an ADS1115.
ESP32_ADC_INPUT_MAX_V = 3.3
ESP32_ADC_PIN_MAX_V = 3.6

#: Divider guard margin (V): a declared limit still holds when the divider
#: arithmetic lands a few millivolts above it, so `adc_input_max: 3.33` accepts
#: the 5 V * (20/30) = 3.3333 V of a 10k/20k divider.
ADC_LIMIT_MARGIN_V = 0.01
