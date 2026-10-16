"""``mics_5524_gas_sensor`` - ESPHome platform for the MiCS-5524 gas sensor.

Two conversion models are available (constants and sources in ``coefficients.py``):

``conversion: dfrobot`` (default)
    Vendor model of the DFRobot_MICS library (MIT) used by the analog breakout.  The
    reference value is captured in clean air, so the board's load resistor and its
    divider do not matter.

``conversion: datasheet``
    ``RS = (VCC * RL) / V_AO - RL``, ``ppm = a * (RS / R0)^b`` - the classic datasheet
    fit, shipped for CO only (``a = 6.3``, ``b = -1.1``).

The analog signal comes either from an existing voltage sampler (``voltage:``) or from
a pin this component owns (``pin:``, which creates an internal ``adc`` sensor for you).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mics_5524_gas_sensor"

# No ``AUTO_LOAD``: for a platform component ESPHome loads what ``sensor.py``
# declares (``sensor``, ``voltage_sampler``, and ``adc`` for the internal ADC sensor
# of a ``pin:`` configuration).

mics_5524_gas_sensor_ns = cg.esphome_ns.namespace("mics_5524_gas_sensor")

# C++ classes: ``MiCS5524Component`` reads the analog output and hands its ratio to
# every gas sub-sensor, ``MiCS5524GasSensor`` converts it with the curve and the
# coefficients of its own gas.
MiCS5524Component = mics_5524_gas_sensor_ns.class_(
    "MiCS5524Component", cg.PollingComponent
)
MiCS5524GasSensor = mics_5524_gas_sensor_ns.class_("MiCS5524GasSensor", sensor.Sensor)

# Configuration keys local to this component; the keys shared with other components
# (`warmup_time`, `enable_pin`, `voltage_divider`) are imported from
# esphome.const by the platform schema, while `r1`/`r2` below are the local keys
# of the `voltage_divider:` mapping itself.
CONF_CONVERSION = "conversion"
CONF_COEFFICIENT_A = "coefficient_a"
CONF_COEFFICIENT_B = "coefficient_b"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_ADC_INPUT_MAX = "adc_input_max"
CONF_ADC_PIN_MAX = "adc_pin_max"
CONF_ADC_ATTENUATION = "adc_attenuation"
CONF_ADC_SAMPLES = "adc_samples"
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

#: Conversion models, mirroring ``micsmath::ConversionModel``.
CONVERSION_MODELS = {
    "dfrobot": 0,  # DFRobot_MICS vendor model (analog breakout)
    "datasheet": 1,  # RS/R0 power law fitted on the datasheet curve
}

#: Divider guard limits of the classic ESP32: the recommended maximum input voltage
#: (VDD) and the datasheet's absolute maximum (VDD + 0.3 V).  Override them per sensor
#: for other samplers, e.g. `adc_input_max: 6.144` for an ADS1115.
ESP32_ADC_INPUT_MAX_V = 3.3
ESP32_ADC_PIN_MAX_V = 3.6

#: Divider guard margin (V): a declared limit still holds when the divider arithmetic
#: lands a few millivolts above it, so `adc_input_max: 3.33` accepts the
#: 5 V * (20/30) = 3.3333 V of a 10k/20k divider.
ADC_LIMIT_MARGIN_V = 0.01
