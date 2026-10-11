"""``mics_5524_gas_sensor`` - ESPHome platform for the MiCS-5524 gas sensor.

The conversion is the vendor model of the DFRobot_MICS library (MIT) used by the
analog breakout (constants and source in ``coefficients.py``):
``ratio = (VCC - V_AO) / (VCC - V_AO_air)``, ``ppm = (threshold - ratio) / gain``
per gas.  The reference value is captured in clean air, so the board's load
resistor and its divider do not matter.

The analog signal comes from an existing voltage sampler (``voltage:``, e.g. an
``adc`` sensor configured next to this one).
"""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mics_5524_gas_sensor"

# No ``AUTO_LOAD``: for a platform component ESPHome loads what ``sensor.py``
# declares (``sensor`` and ``voltage_sampler``).

mics_5524_gas_sensor_ns = cg.esphome_ns.namespace("mics_5524_gas_sensor")

# C++ class: class MiCS5524GasSensor : public sensor::Sensor, public PollingComponent
MiCS5524GasSensor = mics_5524_gas_sensor_ns.class_(
    "MiCS5524GasSensor", sensor.Sensor, cg.PollingComponent
)

# Configuration keys local to this component; `warmup_time` is imported from
# esphome.const by the platform schema.
CONF_GAS = "gas"
CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_VCC = "vcc"
CONF_AIR_REFERENCE = "air_reference"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MIN_PPM = "min_ppm"
CONF_MAX_PPM = "max_ppm"

