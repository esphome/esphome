"""MiCS-5524 gas sensor platform: the DFRobot_MICS vendor model on a voltage sampler."""

import esphome.codegen as cg
from esphome.components import sensor

CODEOWNERS = ["@nliaudat"]
DOMAIN = "mics_5524"

mics_5524_ns = cg.esphome_ns.namespace("mics_5524")

MiCS5524Component = mics_5524_ns.class_("MiCS5524Component", cg.PollingComponent)
MiCS5524GasSensor = mics_5524_ns.class_("MiCS5524GasSensor", sensor.Sensor)

CONF_VOLTAGE_MULTIPLIER = "voltage_multiplier"
CONF_VCC = "vcc"
CONF_AIR_REFERENCE = "air_reference"
CONF_SAMPLE_COUNT = "sample_count"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_MAX_PPM = "max_ppm"
