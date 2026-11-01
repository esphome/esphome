from collections.abc import Callable
from typing import Any

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_ADC_AVERAGING, CONF_ADC_TIME
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
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
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

CONF_ALERT = "alert"
CONF_FUNCTION = "function"
CONF_LIMIT = "limit"
CONF_ACTIVE_HIGH = "active_high"
CONF_LATCH = "latch"
CONF_CONVERSION_READY = "conversion_ready"

ina226_ns = cg.esphome_ns.namespace("ina226")
INA226Component = ina226_ns.class_(
    "INA226Component", cg.PollingComponent, i2c.I2CDevice
)

AlertFunction = ina226_ns.enum("AlertFunction")
ALERT_FUNCTIONS = {
    "none": AlertFunction.ALERT_FUNCTION_NONE,
    "shunt_over": AlertFunction.ALERT_FUNCTION_SHUNT_OVER,
    "shunt_under": AlertFunction.ALERT_FUNCTION_SHUNT_UNDER,
    "bus_over": AlertFunction.ALERT_FUNCTION_BUS_OVER,
    "bus_under": AlertFunction.ALERT_FUNCTION_BUS_UNDER,
    "power_over": AlertFunction.ALERT_FUNCTION_POWER_OVER,
}

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


def validate_alert_config(config):
    """Validate alert configuration to ensure proper usage."""
    function = config.get(CONF_FUNCTION, "none")

    # If function is not none, limit should be explicitly set
    if function != "none":
        if CONF_LIMIT not in config:
            raise cv.Invalid(
                f"alert.limit is required when alert.function is '{function}' (not 'none'). "
                f"Please specify a threshold value for the alert."
            )

        limit = config[CONF_LIMIT]

        # Range validation per function
        if function in ["bus_over", "bus_under"]:
            if limit < 0:
                raise cv.Invalid(
                    f"alert.limit must be non-negative for bus voltage alerts, got {limit}V"
                )
            if limit > 36.0:  # INA226 max bus voltage
                raise cv.Invalid(
                    f"alert.limit exceeds maximum bus voltage (36V), got {limit}V"
                )
        elif function == "power_over":
            if limit < 0:
                raise cv.Invalid(
                    f"alert.limit must be non-negative for power alert, got {limit}W"
                )
        elif function in ["shunt_over", "shunt_under"]:
            # INA226 shunt voltage range: ±81.92mV
            # Negative values are valid for reverse current detection
            if limit < -0.08192 or limit > 0.08192:
                raise cv.Invalid(
                    f"alert.limit must be within shunt voltage range (±81.92mV), got {limit * 1000:.3f}mV"
                )

    return config


def validate_adc_time(value: Any) -> int:
    value = cv.positive_time_period_microseconds(value).total_microseconds
    return cv.enum(ADC_TIMES, int=True)(value)


def _per_channel(validator: Callable[[Any], Any]) -> Callable[[Any], ConfigType]:
    """One value for both channels, or one each; always returns the per-channel dict."""
    return cv.Any(
        cv.All(validator, lambda value: {CONF_VOLTAGE: value, CONF_CURRENT: value}),
        cv.Schema(
            {
                cv.Required(CONF_VOLTAGE): validator,
                cv.Required(CONF_CURRENT): validator,
            }
        ),
    )


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
            cv.Optional(CONF_ADC_TIME, default="1100 us"): _per_channel(
                validate_adc_time
            ),
            cv.Optional(CONF_ADC_AVERAGING, default=4): cv.enum(
                ADC_AVG_SAMPLES, int=True
            ),
            cv.Optional(
                CONF_ALERT,
                default={},
            ): cv.All(
                cv.Schema(
                    {
                        cv.Optional(CONF_FUNCTION, default="none"): cv.enum(
                            ALERT_FUNCTIONS
                        ),
                        cv.Optional(CONF_LIMIT): cv.float_,
                        cv.Optional(CONF_CONVERSION_READY, default=False): cv.boolean,
                        cv.Optional(CONF_ACTIVE_HIGH, default=False): cv.boolean,
                        cv.Optional(CONF_LATCH, default=False): cv.boolean,
                    }
                ),
                validate_alert_config,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_shunt_resistance_ohm(config[CONF_SHUNT_RESISTANCE]))
    cg.add(var.set_max_current_a(config[CONF_MAX_CURRENT]))

    adc_time = config[CONF_ADC_TIME]
    cg.add(var.set_adc_time_voltage(adc_time[CONF_VOLTAGE]))
    cg.add(var.set_adc_time_current(adc_time[CONF_CURRENT]))

    cg.add(var.set_adc_avg_samples(config[CONF_ADC_AVERAGING]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BUS_VOLTAGE, var.set_bus_voltage_sensor)
    await sensors(CONF_SHUNT_VOLTAGE, var.set_shunt_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_POWER, var.set_power_sensor)

    alert_conf = config.get(CONF_ALERT)
    if alert_conf:
        cg.add(var.set_alert_function(alert_conf[CONF_FUNCTION]))
        if CONF_LIMIT in alert_conf:
            cg.add(var.set_alert_limit(alert_conf[CONF_LIMIT]))
        else:
            # Set default limit of 0.0 when function is "none"
            cg.add(var.set_alert_limit(0.0))
        cg.add(var.set_alert_conversion_ready(alert_conf[CONF_CONVERSION_READY]))
        cg.add(var.set_alert_polarity(alert_conf[CONF_ACTIVE_HIGH]))
        cg.add(var.set_alert_latch(alert_conf[CONF_LATCH]))


CLEAR_ALERT_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(INA226Component),
    },
)


automation.register_apply_action(
    "ina226.clear_alert",
    CLEAR_ALERT_ACTION_SCHEMA,
    automation.ApplyCall("clear_alert_flag()"),
)
