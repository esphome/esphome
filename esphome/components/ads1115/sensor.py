import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
    CONF_ID,
)
from . import ads1115_ns, ADS1115Component

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


def validate_gain(value):
    if isinstance(value, float):
        value = f"{value:0.03f}"
    elif not isinstance(value, str):
        raise cv.Invalid(f'invalid gain "{value}"')

    return cv.enum(GAIN)(value)


ADS1115Sensor = ads1115_ns.class_(
    "ADS1115Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONF_ADS1115_ID = "ads1115_id"
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ADS1115Sensor),
            cv.GenerateID(CONF_ADS1115_ID): cv.use_id(ADS1115Component),
            cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
            cv.Required(CONF_GAIN): validate_gain,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_ADS1115_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
    cg.add(var.set_gain(config[CONF_GAIN]))

    cg.add(paren.register_sensor(var))
