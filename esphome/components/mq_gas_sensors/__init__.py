"""MQ gas sensor library for ESPHome.

A native ESPHome port of the MQUnifiedsensor PPM model
(``PPM = a * ratio^b`` with ``ratio = RS / R0``) for the sensor types with
built-in curves (see ``coefficients.py``).

The analog signal is either taken from an existing ESPHome voltage-sampler
sensor (``voltage:``) or from a pin owned by this component (``pin:``, which
creates a hidden, internal ``adc`` sensor for you).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mq_gas_sensors"

# No ``AUTO_LOAD`` here: for a platform component ESPHome processes the list of the
# platform module, and ``sensor.py`` declares ``sensor``, ``voltage_sampler``,
# ``adc`` (for the internal ADC sensor of a ``pin:`` configuration) and
# ``text_sensor`` (for the log mirror).

mq_gas_sensors_ns = cg.esphome_ns.namespace("mq_gas_sensors")

# C++ class: class MQGasSensor : public sensor::Sensor, public PollingComponent
MQGasSensor = mq_gas_sensors_ns.class_(
    "MQGasSensor", sensor.Sensor, cg.PollingComponent
)

# ---------------------------------------------------------------------------
# Configuration keys
# ---------------------------------------------------------------------------
CONF_SENSOR_TYPE = "sensor_type"
CONF_GAS = "gas"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_R1 = "r1"
CONF_R2 = "r2"
CONF_ADC_INPUT_MAX = "adc_input_max"
CONF_ADC_PIN_MAX = "adc_pin_max"
CONF_ADC_ATTENUATION = "adc_attenuation"
CONF_ADC_SAMPLES = "adc_samples"
CONF_RL = "rl"
CONF_R0 = "r0"
CONF_VCC = "vcc"
CONF_COEFFICIENT_A = "coefficient_a"
CONF_COEFFICIENT_B = "coefficient_b"
CONF_REGRESSION_METHOD = "regression_method"
CONF_RATIO_MODE = "ratio_mode"
CONF_RATIO_IN_CLEAN_AIR = "ratio_in_clean_air"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"
CONF_CORRECTION_FACTOR = "correction_factor"
CONF_LOG_SENSOR = "log_sensor"
CONF_LOG_PPM = "log_ppm"
CONF_RATIO_SENSOR = "ratio_sensor"
CONF_RS_SENSOR = "rs_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"

# MQUnifiedsensor::setRegressionMethod() values.
REGRESSION_METHODS = {
    "exponential": 1,  # _PPM = a * ratio^b
    "linear": 2,  # log10(_PPM) = (log10(ratio) - b) / a
    "inverse": 3,  # _PPM = (ratio / a)^(1 / b)
}

# MQUnifiedsensor uses R0/RS (see readSensorR0Rs(), "INVERTED for MQ-131"),
# while the published datasheet coefficients are fitted against RS/R0.
RATIO_MODES = {
    "rs_r0": 0,  # datasheet convention (default)
    "r0_rs": 1,  # MQUnifiedsensor readSensorR0Rs() convention
}

# Default ADC input limits for the classic ESP32, used by the divider guard:
# the recommended maximum input voltage (VDD) and the datasheet's absolute
# maximum (VDD + 0.3 V). Override them per sensor for other samplers, e.g.
# `adc_input_max: 6.144` / `adc_pin_max: 6.144` for an ADS1115 at the widest gain.
ESP32_ADC_INPUT_MAX_V = 3.3
ESP32_ADC_PIN_MAX_V = 3.6

#: Comparison margin of the divider guard (V): a declared limit is honoured even
#: when the divider arithmetic lands a few millivolts above it, so that
#: `adc_input_max: 3.33` accepts the 5 V * (20/30) = 3.3333 V of a 10k/20k divider.
ADC_LIMIT_MARGIN_V = 0.01
