from collections.abc import Callable
import math
from typing import Any

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_ACTIVE_HIGH, CONF_ADC_AVERAGING, CONF_ADC_TIME
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
CONF_LATCH = "latch"
CONF_CONVERSION_READY = "conversion_ready"

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


# MASK_ENABLE function bit and the ALERT_LIMIT LSB (V, V or W per count, power from the current LSB)
ALERT_FUNCTION_BITS = {
    "shunt_over": 1 << 15,
    "shunt_under": 1 << 14,
    "bus_over": 1 << 13,
    "bus_under": 1 << 12,
    "power_over": 1 << 11,
}
MASK_CONVERSION_READY = 1 << 10
MASK_ACTIVE_HIGH = 1 << 1
MASK_LATCH = 1 << 0
SHUNT_LSB_V = 2.5e-6
BUS_LSB_V = 1.25e-3

POWER = cv.float_with_unit("power", "(w|W|watt|Watts)?")

_ALERT_FLAGS = {
    cv.Optional(CONF_CONVERSION_READY, default=False): cv.boolean,
    cv.Optional(CONF_ACTIVE_HIGH, default=False): cv.boolean,
    cv.Optional(CONF_LATCH, default=False): cv.boolean,
}
_SHUNT_LIMIT = {cv.Required(CONF_LIMIT): cv.voltage, **_ALERT_FLAGS}
_BUS_LIMIT = {
    cv.Required(CONF_LIMIT): cv.All(cv.voltage, cv.Range(min=0.0, max=36.0)),
    **_ALERT_FLAGS,
}
ALERT_SCHEMA = cv.typed_schema(
    {
        "none": cv.Schema(_ALERT_FLAGS),
        "shunt_over": cv.Schema(_SHUNT_LIMIT),
        "shunt_under": cv.Schema(_SHUNT_LIMIT),
        "bus_over": cv.Schema(_BUS_LIMIT),
        "bus_under": cv.Schema(_BUS_LIMIT),
        "power_over": cv.Schema(
            {cv.Required(CONF_LIMIT): cv.All(POWER, cv.Range(min=0.0)), **_ALERT_FLAGS}
        ),
    },
    key=CONF_FUNCTION,
    default_type="none",
)


def _alert_limit_register(config: ConfigType) -> int:
    """The ALERT_LIMIT value as a signed count; range checked by the caller."""
    alert = config[CONF_ALERT]
    function = alert[CONF_FUNCTION]
    if function == "none":
        return 0
    if function.startswith("shunt"):
        lsb = SHUNT_LSB_V
    elif function.startswith("bus"):
        lsb = BUS_LSB_V
    else:
        # The power register counts 25 current LSBs
        lsb = math.ceil(config[CONF_MAX_CURRENT] * 1e6 / 32768) * 25 / 1e6
    return round(alert[CONF_LIMIT] / lsb)


def _validate_alert(config: ConfigType) -> ConfigType:
    if CONF_ALERT not in config:
        return config
    raw = _alert_limit_register(config)
    low = -32768 if config[CONF_ALERT][CONF_FUNCTION].startswith("shunt") else 0
    high = 65535 if config[CONF_ALERT][CONF_FUNCTION] == "power_over" else 32767
    if not low <= raw <= high:
        raise cv.Invalid(
            f"'limit' is out of range; it gives register value {raw}, allowed {low} to {high}",
            path=[CONF_ALERT, CONF_LIMIT],
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


CONFIG_SCHEMA = cv.All(
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
            cv.Optional(CONF_ALERT): ALERT_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40)),
    _validate_alert,
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

    if (alert := config.get(CONF_ALERT)) is not None:
        cg.add_define("USE_INA226_ALERT")
        mask = ALERT_FUNCTION_BITS.get(alert[CONF_FUNCTION], 0)
        if alert[CONF_CONVERSION_READY]:
            mask |= MASK_CONVERSION_READY
        if alert[CONF_ACTIVE_HIGH]:
            mask |= MASK_ACTIVE_HIGH
        if alert[CONF_LATCH]:
            mask |= MASK_LATCH
        cg.add(var.set_alert(_alert_limit_register(config) & 0xFFFF, mask))


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
