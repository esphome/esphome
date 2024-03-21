import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    CONF_RESOLUTION,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    CONF_ID,
)
from . import ads1220_ns, ADS1220Component

DEPENDENCIES = ["ads1220"]

ADS1220Multiplexer = ads1220_ns.enum("ADS1220Multiplexer")
MUX = {
    "A0_A1": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N1,
    "A0_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N2,
    "A0_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_N3,
    "A1_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N2,
    "A1_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N3,
    "A2_A3": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P2_N3,
    "A1_A0": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_N0,
    "A3_A2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P3_N2,
    "A0_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P0_NG,
    "A1_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P1_NG,
    "A2_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P2_NG,
    "A3_GND": ADS1220Multiplexer.ADS1220_MULTIPLEXER_P3_NG,
    "REFPX_REFNX_4": ADS1220Multiplexer.ADS1220_MULTIPLEXER_REFPX_REFNX_4,
    "AVDD_M_AVSS_4": ADS1220Multiplexer.ADS1220_MULTIPLEXER_AVDD_M_AVSS_4,
    "AVDD_P_AVSS_2": ADS1220Multiplexer.ADS1220_MULTIPLEXER_AVDD_P_AVSS_2,
}

ADS1220Gain = ads1220_ns.enum("ADS1220Gain")
GAIN = {
    "1": ADS1220Gain.ADS1220_GAIN_1,
    "2": ADS1220Gain.ADS1220_GAIN_2,
    "4": ADS1220Gain.ADS1220_GAIN_4,
    "8": ADS1220Gain.ADS1220_GAIN_8,
    "16": ADS1220Gain.ADS1220_GAIN_16,
    "32": ADS1220Gain.ADS1220_GAIN_32,
    "64": ADS1220Gain.ADS1220_GAIN_64,
    "128": ADS1220Gain.ADS1220_GAIN_128,
}

ADS1220Resolution = ads1220_ns.enum("ADS1220Resolution")
RESOLUTION = {
    "24_BITS": ADS1220Resolution.ADS1220_24_BITS,
}


def validate_gain(value):
    if isinstance(value, float):
        value = f"{value:0.03f}"
    elif not isinstance(value, str):
        raise cv.Invalid(f'invalid gain "{value}"')

    return cv.enum(GAIN)(value)


ADS1220Sensor = ads1220_ns.class_(
    "ADS1220Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONF_ADS1220_ID = "ads1220_id"
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS1220Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS1220_ID): cv.use_id(ADS1220Component),
            cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
            cv.Required(CONF_GAIN): validate_gain,
            cv.Optional(CONF_RESOLUTION, default="24_BITS"): cv.enum(
                RESOLUTION, upper=True, space="_"
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_ADS1220_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))

    cg.add(paren.register_sensor(var))
