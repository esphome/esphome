import math

import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
from esphome.components.ads1xxx import MUX_GAIN_SCHEMA, mux_gain_bits
import esphome.config_validation as cv
from esphome.const import (
    CONF_RESOLUTION,
    CONF_SAMPLE_RATE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_ADS1115_ID, ADS1115Component, ads1115_ns

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["ads1115"]

RESOLUTION = {"16_BITS": 16, "12_BITS": 12}
# Samples per second of each data rate code; the ADS1015 (12 bits) runs faster
DATA_RATES = {
    16: (8, 16, 32, 64, 128, 250, 475, 860),
    12: (128, 250, 490, 920, 1600, 2400, 3300, 3300),
}
SAMPLERATE = {str(sps): i for i, sps in enumerate(DATA_RATES[16])}

ADS1115Sensor = ads1115_ns.class_(
    "ADS1115Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS1115Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS1115_ID): cv.use_id(ADS1115Component),
            **MUX_GAIN_SCHEMA,
            cv.Optional(CONF_RESOLUTION, default="16_BITS"): cv.enum(
                RESOLUTION, upper=True, space="_"
            ),
            cv.Optional(CONF_SAMPLE_RATE, default="860"): cv.enum(
                SAMPLERATE, string=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    rate = config[CONF_SAMPLE_RATE].enum_value
    sps = DATA_RATES[config[CONF_RESOLUTION].enum_value][rate]
    delay_ms = math.ceil(1000 / sps + 0.5)
    var = await sensor.new_sensor(config, mux_gain_bits(config) | rate << 5, delay_ms)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS1115_ID])
