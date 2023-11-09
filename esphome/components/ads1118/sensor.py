import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
    CONF_ID,
    CONF_TYPE,
)
from . import ads1118_ns, ADS1118

DEPENDENCIES = ["ads1118"]

ADS1118Multiplexer = ads1118_ns.enum("ADS1118Multiplexer")
MUX = {
    "A0_A1": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P0_N1,
    "A0_A3": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P0_N3,
    "A1_A3": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P1_N3,
    "A2_A3": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P2_N3,
    "A0_GND": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P0_NG,
    "A1_GND": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P1_NG,
    "A2_GND": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P2_NG,
    "A3_GND": ADS1118Multiplexer.ADS1118_MULTIPLEXER_P3_NG,
}

ADS1118Gain = ads1118_ns.enum("ADS1118Gain")
GAIN = {
    "6.144": ADS1118Gain.ADS1118_GAIN_6P144,
    "4.096": ADS1118Gain.ADS1118_GAIN_4P096,
    "2.048": ADS1118Gain.ADS1118_GAIN_2P048,
    "1.024": ADS1118Gain.ADS1118_GAIN_1P024,
    "0.512": ADS1118Gain.ADS1118_GAIN_0P512,
    "0.256": ADS1118Gain.ADS1118_GAIN_0P256,
}


def validate_gain(value):
    if isinstance(value, float):
        value = f"{value:0.03f}"
    elif not isinstance(value, str):
        raise cv.Invalid(f'invalid gain "{value}"')

    return cv.enum(GAIN)(value)


ADS1118Sensor = ads1118_ns.class_(
    "ADS1118Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)
CONF_ADS1118_ID = "ads1118_id"
TYPE_ADC = "adc"
TYPE_TEMPERATURE = "temperature"

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_ADC: sensor.sensor_schema(
            ADS1118Sensor,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(
            {
                cv.GenerateID(CONF_ADS1118_ID): cv.use_id(ADS1118),
                cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
                cv.Required(CONF_GAIN): validate_gain,
            }
        )
        .extend(cv.polling_component_schema("60s")),
        TYPE_TEMPERATURE: sensor.sensor_schema(
            ADS1118Sensor,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(
            {
                cv.GenerateID(CONF_ADS1118_ID): cv.use_id(ADS1118),
            }
        )
        .extend(cv.polling_component_schema("60s")),
    },
    default_type=TYPE_ADC,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ADS1118_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)

    if config[CONF_TYPE] == TYPE_ADC:
        await sensor.register_sensor(var, config)
        cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
        cg.add(var.set_gain(config[CONF_GAIN]))
        cg.add(parent.register_sensor(var))
    if config[CONF_TYPE] == TYPE_TEMPERATURE:
        await sensor.register_sensor(var, config)
        cg.add(var.set_temperature_mode(True))
        cg.add(parent.register_sensor(var))
