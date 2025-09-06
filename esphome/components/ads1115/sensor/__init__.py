import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_MULTIPLEXER,
    CONF_RESOLUTION,
    CONF_SAMPLE_RATE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

from .. import CONF_ADS1115_ID, ADS1115Component, ads1115_ns

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["ads1115"]

ADS1115Multiplexer = ads1115_ns.enum("ADS1115Multiplexer")
MUX = {
    "A0_A1": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_N1,
    "A0_A3": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_N3,
    "A1_A3": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P1_N3,
    "A2_A3": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P2_N3,
    "A0_GND": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_NG,
    "A1_GND": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P1_NG,
    "A2_GND": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P2_NG,
    "A3_GND": ADS1115Multiplexer.ADS1115_MULTIPLEXER_P3_NG,
}

ADS1115Gain = ads1115_ns.enum("ADS1115Gain")
GAIN = {
    "6.144": ADS1115Gain.ADS1115_GAIN_6P144,
    "4.096": ADS1115Gain.ADS1115_GAIN_4P096,
    "2.048": ADS1115Gain.ADS1115_GAIN_2P048,
    "1.024": ADS1115Gain.ADS1115_GAIN_1P024,
    "0.512": ADS1115Gain.ADS1115_GAIN_0P512,
    "0.256": ADS1115Gain.ADS1115_GAIN_0P256,
}

ADS1115Resolution = ads1115_ns.enum("ADS1115Resolution")
RESOLUTION = {
    "16_BITS": ADS1115Resolution.ADS1115_16_BITS,
    "12_BITS": ADS1115Resolution.ADS1015_12_BITS,
}

ADS1115Samplerate = ads1115_ns.enum("ADS1115Samplerate")
SAMPLERATE = {
    "8": ADS1115Samplerate.ADS1115_8SPS,
    "16": ADS1115Samplerate.ADS1115_16SPS,
    "32": ADS1115Samplerate.ADS1115_32SPS,
    "64": ADS1115Samplerate.ADS1115_64SPS,
    "128": ADS1115Samplerate.ADS1115_128SPS,
    "250": ADS1115Samplerate.ADS1115_250SPS,
    "475": ADS1115Samplerate.ADS1115_475SPS,
    "860": ADS1115Samplerate.ADS1115_860SPS,
}

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
            cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
            cv.Required(CONF_GAIN): cv.enum(GAIN, string=True),
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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS1115_ID])

    cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_samplerate(config[CONF_SAMPLE_RATE]))
