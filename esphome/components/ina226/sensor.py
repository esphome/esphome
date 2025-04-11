import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_MAX_CURRENT,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
    UNIT_WATT,
)

DEPENDENCIES = ["i2c"]

CONF_ADC_AVERAGING = "adc_averaging"
CONF_ADC_TIME = "adc_time"

ina226_ns = cg.esphome_ns.namespace("ina226")
INA226Component = ina226_ns.class_(
    "INA226Component", cg.PollingComponent, i2c.I2CDevice
)

AdcTime = ina226_ns.enum("AdcTime")
ADC_TIMES = {
    140: AdcTime.ADC_TIME_140US,
    204: AdcTime.ADC_TIME_204US,
    332: AdcTime.ADC_TIME_332US,
    588: AdcTime.ADC_TIME_588US,
    1100: AdcTime.ADC_TIME_1100US,
    2116: AdcTime.ADC_TIME_2116US,
    4156: AdcTime.ADC_TIME_4156US,
    8244: AdcTime.ADC_TIME_8244US,
}

AdcAvgSamples = ina226_ns.enum("AdcAvgSamples")
ADC_AVG_SAMPLES = {
    1: AdcAvgSamples.ADC_AVG_SAMPLES_1,
    4: AdcAvgSamples.ADC_AVG_SAMPLES_4,
    16: AdcAvgSamples.ADC_AVG_SAMPLES_16,
    64: AdcAvgSamples.ADC_AVG_SAMPLES_64,
    128: AdcAvgSamples.ADC_AVG_SAMPLES_128,
    256: AdcAvgSamples.ADC_AVG_SAMPLES_256,
    512: AdcAvgSamples.ADC_AVG_SAMPLES_512,
    1024: AdcAvgSamples.ADC_AVG_SAMPLES_1024,
}


def validate_adc_time(value):
    value = cv.positive_time_period_microseconds(value).total_microseconds
    return cv.enum(ADC_TIMES, int=True)(value)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA226Component),
            cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SHUNT_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
                cv.resistance, cv.Range(min=0.0)
            ),
            cv.Optional(CONF_MAX_CURRENT, default=3.2): cv.All(
                cv.current, cv.Range(min=0.0)
            ),
            cv.Optional(CONF_ADC_TIME, default="1100 us"): cv.Any(
                validate_adc_time,
                cv.Schema(
                    {
                        cv.Required(CONF_VOLTAGE): validate_adc_time,
                        cv.Required(CONF_CURRENT): validate_adc_time,
                    }
                ),
            ),
            cv.Optional(CONF_ADC_AVERAGING, default=4): cv.enum(
                ADC_AVG_SAMPLES, int=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))

    adc_time_config = config[CONF_ADC_TIME]
    if isinstance(adc_time_config, dict):
        cg.add(var.set_adc_time_voltage(adc_time_config[CONF_VOLTAGE]))
        cg.add(var.set_adc_time_current(adc_time_config[CONF_CURRENT]))
    else:
        cg.add(var.set_adc_time_voltage(adc_time_config))
        cg.add(var.set_adc_time_current(adc_time_config))

    cg.add(var.set_adc_avg_samples(config[CONF_ADC_AVERAGING]))

    if CONF_BUS_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BUS_VOLTAGE])
        cg.add(var.set_bus_voltage_sensor(sens))

    if CONF_SHUNT_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_SHUNT_VOLTAGE])
        cg.add(var.set_shunt_voltage_sensor(sens))

    if CONF_CURRENT in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))

    if CONF_POWER in config:
        sens = await sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))
