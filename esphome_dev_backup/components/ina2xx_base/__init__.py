import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ENERGY,
    CONF_MAX_CURRENT,
    CONF_MODEL,
    CONF_NAME,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

CODEOWNERS = ["@latonita"]

CONF_ADC_AVERAGING = "adc_averaging"
CONF_ADC_RANGE = "adc_range"
CONF_ADC_TIME = "adc_time"
CONF_CHARGE = "charge"
CONF_CHARGE_COULOMBS = "charge_coulombs"
CONF_ENERGY_JOULES = "energy_joules"
CONF_TEMPERATURE_COEFFICIENT = "temperature_coefficient"
UNIT_AMPERE_HOURS = "Ah"
UNIT_COULOMB = "C"
UNIT_JOULE = "J"
UNIT_MILLIVOLT = "mV"

ina2xx_base_ns = cg.esphome_ns.namespace("ina2xx_base")
INA2XX = ina2xx_base_ns.class_("INA2XX", cg.PollingComponent)

AdcTime = ina2xx_base_ns.enum("AdcTime")
ADC_TIMES = {
    50: AdcTime.ADC_TIME_50US,
    84: AdcTime.ADC_TIME_84US,
    150: AdcTime.ADC_TIME_150US,
    280: AdcTime.ADC_TIME_280US,
    540: AdcTime.ADC_TIME_540US,
    1052: AdcTime.ADC_TIME_1052US,
    2074: AdcTime.ADC_TIME_2074US,
    4120: AdcTime.ADC_TIME_4120US,
}

AdcAvgSamples = ina2xx_base_ns.enum("AdcAvgSamples")
ADC_SAMPLES = {
    1: AdcAvgSamples.ADC_AVG_SAMPLES_1,
    4: AdcAvgSamples.ADC_AVG_SAMPLES_4,
    16: AdcAvgSamples.ADC_AVG_SAMPLES_16,
    64: AdcAvgSamples.ADC_AVG_SAMPLES_64,
    128: AdcAvgSamples.ADC_AVG_SAMPLES_128,
    256: AdcAvgSamples.ADC_AVG_SAMPLES_256,
    512: AdcAvgSamples.ADC_AVG_SAMPLES_512,
    1024: AdcAvgSamples.ADC_AVG_SAMPLES_1024,
}

SENSOR_MODEL_OPTIONS = {
    CONF_ENERGY: ["INA228", "INA229"],
    CONF_ENERGY_JOULES: ["INA228", "INA229"],
    CONF_CHARGE: ["INA228", "INA229"],
    CONF_CHARGE_COULOMBS: ["INA228", "INA229"],
}


def validate_model_config(config):
    model = config[CONF_MODEL]

    for key in config:
        if key in SENSOR_MODEL_OPTIONS:
            if model not in SENSOR_MODEL_OPTIONS[key]:
                raise cv.Invalid(
                    f"Device model '{model}' does not support '{key}' sensor"
                )

    tempco = config[CONF_TEMPERATURE_COEFFICIENT]
    if tempco > 0 and model not in ["INA228", "INA229"]:
        raise cv.Invalid(
            f"Device model '{model}' does not support temperature coefficient"
        )

    return config


def validate_adc_time(value):
    value = cv.positive_time_period_microseconds(value).total_microseconds
    return cv.enum(ADC_TIMES, int=True)(value)


INA2XX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SHUNT_RESISTANCE): cv.All(cv.resistance, cv.Range(min=0.0)),
        cv.Required(CONF_MAX_CURRENT): cv.All(cv.current, cv.Range(min=0.0)),
        cv.Optional(CONF_ADC_RANGE, default=0): cv.int_range(min=0, max=1),
        cv.Optional(CONF_ADC_TIME, default="4120 us"): cv.Any(
            validate_adc_time,
            {
                cv.Optional(CONF_BUS_VOLTAGE, default="4120 us"): validate_adc_time,
                cv.Optional(CONF_SHUNT_VOLTAGE, default="4120 us"): validate_adc_time,
                cv.Optional(CONF_TEMPERATURE, default="4120 us"): validate_adc_time,
            },
        ),
        cv.Optional(CONF_ADC_AVERAGING, default=128): cv.enum(ADC_SAMPLES, int=True),
        cv.Optional(CONF_TEMPERATURE_COEFFICIENT, default=0): cv.int_range(
            min=0, max=16383
        ),
        cv.Optional(CONF_SHUNT_VOLTAGE): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIVOLT,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_BUS_VOLTAGE): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_TEMPERATURE): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_CURRENT): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=8,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_POWER): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=6,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_ENERGY): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT_HOURS,
                accuracy_decimals=8,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_ENERGY_JOULES): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_JOULE,
                accuracy_decimals=8,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_CHARGE): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE_HOURS,
                accuracy_decimals=8,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_CHARGE_COULOMBS): cv.maybe_simple_value(
            sensor.sensor_schema(
                unit_of_measurement=UNIT_COULOMB,
                accuracy_decimals=8,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            key=CONF_NAME,
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def setup_ina2xx(var, config):
    await cg.register_component(var, config)

    cg.add(var.set_model(config[CONF_MODEL]))

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))
    cg.add(var.set_adc_range(config[CONF_ADC_RANGE]))
    cg.add(var.set_adc_avg_samples(config[CONF_ADC_AVERAGING]))
    cg.add(var.set_shunt_tempco(config[CONF_TEMPERATURE_COEFFICIENT]))

    adc_time_config = config[CONF_ADC_TIME]
    if isinstance(adc_time_config, dict):
        cg.add(var.set_adc_time_bus_voltage(adc_time_config[CONF_BUS_VOLTAGE]))
        cg.add(var.set_adc_time_shunt_voltage(adc_time_config[CONF_SHUNT_VOLTAGE]))
        cg.add(var.set_adc_time_die_temperature(adc_time_config[CONF_TEMPERATURE]))
    else:
        cg.add(var.set_adc_time_bus_voltage(adc_time_config))
        cg.add(var.set_adc_time_shunt_voltage(adc_time_config))
        cg.add(var.set_adc_time_die_temperature(adc_time_config))

    if conf := config.get(CONF_SHUNT_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_shunt_voltage_sensor(sens))

    if conf := config.get(CONF_BUS_VOLTAGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_bus_voltage_sensor(sens))

    if conf := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_die_temperature_sensor(sens))

    if conf := config.get(CONF_CURRENT):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_current_sensor(sens))

    if conf := config.get(CONF_POWER):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_power_sensor(sens))

    if conf := config.get(CONF_ENERGY):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_wh(sens))

    if conf := config.get(CONF_ENERGY_JOULES):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_energy_sensor_j(sens))

    if conf := config.get(CONF_CHARGE):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_sensor_ah(sens))

    if conf := config.get(CONF_CHARGE_COULOMBS):
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_charge_sensor_c(sens))
