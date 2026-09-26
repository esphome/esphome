"""MQ gas sensor library for ESPHome.

A native ESPHome port of the MQUnifiedsensor PPM model used by the
`MQSensorsLib` / `esp-iot-solution MQSensorLIB` ESP-IDF component
(https://github.com/RapportTecnologia/esp-iot-solution/tree/MQSensorLib/components/sensors/gas/MQSensorLIB).

It supports every MQ sensor of the MQUnifiedsensor family:

    MQ-2, MQ-3, MQ-4, MQ-5, MQ-6, MQ-7, MQ-8, MQ-9, MQ-131, MQ-135,
    MQ-136, MQ-137, MQ-138, MQ-214, MQ-303A, MQ-309A and CUSTOM.

The analog signal is either taken from an existing ESPHome voltage-sampler
sensor (``voltage:``) or from a pin owned by this component (``pin:``, which
creates a hidden, internal ``adc`` sensor for you).

Optional extras ported from https://github.com/abcdaaaaaaaaa/MQDataScience:

* ``correction_mode: mqdatascience`` compensates the RS/R0 ratio for the
  ambient temperature and humidity (``temperature:``/``humidity:`` sensor ids).
* ``curve: mqdatascience`` selects their ``a`` / ``b`` coefficient dataset
  instead of the SolderedElectronics/MQUnifiedsensor one.
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
AUTO_LOAD = ["sensor", "voltage_sampler"]

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
CONF_REGRESSION_METHOD = "regression_method"
CONF_RATIO_MODE = "ratio_mode"
CONF_RATIO_IN_CLEAN_AIR = "ratio_in_clean_air"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"
CONF_CORRECTION_FACTOR = "correction_factor"
CONF_CORRECTION_MODE = "correction_mode"
CONF_CORRECTION_CLAMP = "correction_clamp"
CONF_CORRECTION_SENSOR = "correction_sensor"
CONF_CURVE = "curve"
CONF_LOG_SENSOR = "log_sensor"
CONF_LOG_PPM = "log_ppm"
CONF_PERSIST = "persist"
CONF_RATIO_SENSOR = "ratio_sensor"
CONF_RS_SENSOR = "rs_sensor"
CONF_VOLTAGE_SENSOR = "voltage_sensor"

# MQUnifiedsensor::setRegressionMethod() values plus the MQDataScience inverse
# form (used by their published MQ-8 H2 coefficients: ppm = (ratio / a)^(1 / b)).
REGRESSION_METHODS = {
    "exponential": 1,  # _PPM = a * ratio^b
    "linear": 2,  # log10(_PPM) = (log10(ratio) - b) / a
    "inverse": 3,  # _PPM = (ratio / a)^(1 / b)
}

# Temperature/humidity compensation of the RS/R0 ratio.
CORRECTION_MODES = {
    "none": 0,  # no compensation (default)
    "mqdatascience": 1,  # a + c * exp(b * T), MQDataScience Correction.cpp
}

# How the corrected PPM value is clamped.
CORRECTION_CLAMPS = {
    # Clip to the configured max_ppm - the alarm ceiling never moves.
    "absolute": 0,
    # Clip to max_ppm * correction - MQDataScience's own behaviour, the ceiling
    # drops with the correction (not recommended for a safety limit).
    "scaled": 1,
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

# MQUnifiedsensor uses R0/RS (see readSensorR0Rs(), "INVERTED for MQ-131"),
# while the published datasheet coefficients are fitted against RS/R0.
RATIO_MODES = {
    "rs_r0": 0,  # datasheet convention (default)
    "r0_rs": 1,  # MQUnifiedsensor readSensorR0Rs() convention
}
