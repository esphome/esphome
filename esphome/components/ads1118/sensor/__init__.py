import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)

from .. import ADS1118, CONF_ADS1118_ID, ads1118_ns

AUTO_LOAD = ["voltage_sampler"]
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


ADS1118Sensor = ads1118_ns.class_(
    "ADS1118Sensor",
    cg.PollingComponent,
    sensor.Sensor,
    voltage_sampler.VoltageSampler,
    cg.Parented.template(ADS1118),
)

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
                cv.Required(CONF_GAIN): cv.enum(GAIN, string=True),
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
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS1118_ID])

    if config[CONF_TYPE] == TYPE_ADC:
        cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
        cg.add(var.set_gain(config[CONF_GAIN]))
    if config[CONF_TYPE] == TYPE_TEMPERATURE:
        cg.add(var.set_temperature_mode(True))
