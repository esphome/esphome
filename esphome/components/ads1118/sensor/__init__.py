import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
from esphome.components.ads1xxx import MUX_GAIN_SCHEMA, mux_gain_bits
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import ADS1118, CONF_ADS1118_ID, ads1118_ns

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["ads1118"]

ADS1118Sensor = ads1118_ns.class_(
    "ADS1118Sensor",
    cg.PollingComponent,
    sensor.Sensor,
    voltage_sampler.VoltageSampler,
    cg.Parented.template(ADS1118),
)

TYPE_ADC = "adc"
TYPE_TEMPERATURE = "temperature"
# Config register TS_MODE bit, ADS1118_CONFIG_TEMPERATURE in C++
CONFIG_TEMPERATURE = 1 << 4

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
                **MUX_GAIN_SCHEMA,
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


async def to_code(config: ConfigType) -> None:
    bits = (
        mux_gain_bits(config) if config[CONF_TYPE] == TYPE_ADC else CONFIG_TEMPERATURE
    )
    var = await sensor.new_sensor(config, bits)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS1118_ID])
