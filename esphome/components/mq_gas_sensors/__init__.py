"""MQ gas sensor library for ESPHome.

A native ESPHome port of the MQUnifiedsensor PPM model
(``PPM = a * ratio^b`` with ``ratio = RS / R0``) for the sensor types with
built-in curves (see ``coefficients.py``).

The analog signal is taken from an existing ESPHome voltage-sampler
sensor (``voltage:``).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mq_gas_sensors"
mq_gas_sensors_ns = cg.esphome_ns.namespace("mq_gas_sensors")

MQGasSensor = mq_gas_sensors_ns.class_(
    "MQGasSensor", sensor.Sensor, cg.PollingComponent
)

CONF_SENSOR_TYPE = "sensor_type"
CONF_GAS = "gas"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_RL = "rl"
CONF_R0 = "r0"
CONF_VCC = "vcc"
CONF_RATIO_IN_CLEAN_AIR = "ratio_in_clean_air"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"

REGRESSION_METHODS = {
    "exponential": 1,  # _PPM = a * ratio^b
    "linear": 2,  # log10(_PPM) = (log10(ratio) - b) / a
    "inverse": 3,  # _PPM = (ratio / a)^(1 / b)
}
