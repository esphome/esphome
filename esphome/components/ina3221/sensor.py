from typing import Any

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_ADC_AVERAGING, CONF_ADC_TIME
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUS_VOLTAGE,
    CONF_CURRENT,
    CONF_ID,
    CONF_POWER,
    CONF_SHUNT_RESISTANCE,
    CONF_SHUNT_VOLTAGE,
    CONF_UPDATE_INTERVAL,
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

CONF_CHANNEL_1 = "channel_1"
CONF_CHANNEL_2 = "channel_2"
CONF_CHANNEL_3 = "channel_3"
CONF_CONTINUOUS_MODE = "continuous_mode"
CONF_WARNING_CURRENT_LIMIT = "warning_current_limit"
CONF_CRITICAL_CURRENT_LIMIT = "critical_current_limit"
CONF_SUM_SHUNT_VOLTAGE = "sum_shunt_voltage"
CONF_SUM_CURRENT = "sum_current"
CONF_SUM_POWER = "sum_power"

CHANNELS = (CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3)

# Configuration register layout: channel enables in bits 14..12, averaging in 11..9,
# bus conversion time in 8..6, shunt conversion time in 5..3, operating mode in 2..0
CHANNEL_ENABLE_BITS = (1 << 14, 1 << 13, 1 << 12)
ADC_AVERAGING_SHIFT = 9
ADC_TIME_BUS_SHIFT = 6
ADC_TIME_SHUNT_SHIFT = 3
MODE_SINGLE_SHOT = 0b011
MODE_CONTINUOUS = 0b111

# Register field value by sample count and by conversion time in microseconds
ADC_AVERAGING = {1: 0, 4: 1, 16: 2, 64: 3, 128: 4, 256: 5, 512: 6, 1024: 7}
ADC_TIMES_US = {140: 0, 204: 1, 332: 2, 588: 3, 1100: 4, 2116: 5, 4156: 6, 8244: 7}

# Alert limits are shunt voltages in 40 uV steps, held in bits 15..3
ALERT_LIMIT_LSB_V = 40e-6
ALERT_LIMIT_MAX_V = 0x0FFF * ALERT_LIMIT_LSB_V
# Headroom on top of the computed conversion time before the results are read
SINGLE_SHOT_MARGIN_MS = 5

ina3221_ns = cg.esphome_ns.namespace("ina3221")
INA3221Component = ina3221_ns.class_(
    "INA3221Component", cg.PollingComponent, i2c.I2CDevice
)

VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)
POWER_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_WATT,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)
CURRENT_LIMIT_SCHEMA = cv.All(cv.current, cv.Range(min=0.0, min_included=False))


def validate_adc_time(value: Any) -> int:
    value = cv.positive_time_period_microseconds(value).total_microseconds
    return cv.one_of(*ADC_TIMES_US, int=True)(value)


def validate_alert_limits(config: ConfigType) -> ConfigType:
    shunt = config[CONF_SHUNT_RESISTANCE]
    for key in (CONF_WARNING_CURRENT_LIMIT, CONF_CRITICAL_CURRENT_LIMIT):
        if (limit := config.get(key)) is not None and limit * shunt > ALERT_LIMIT_MAX_V:
            raise cv.Invalid(
                f"{key} times shunt_resistance exceeds the "
                f"{ALERT_LIMIT_MAX_V * 1000:.1f} mV alert range",
                path=[key],
            )
    return config


INA3221_CHANNEL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_BUS_VOLTAGE): VOLTAGE_SCHEMA,
            cv.Optional(CONF_SHUNT_VOLTAGE): VOLTAGE_SCHEMA,
            cv.Optional(CONF_CURRENT): CURRENT_SCHEMA,
            cv.Optional(CONF_POWER): POWER_SCHEMA,
            cv.Optional(CONF_SHUNT_RESISTANCE, default=0.1): cv.All(
                cv.resistance, cv.Range(min=0.0, max=32.0)
            ),
            cv.Optional(CONF_WARNING_CURRENT_LIMIT): CURRENT_LIMIT_SCHEMA,
            cv.Optional(CONF_CRITICAL_CURRENT_LIMIT): CURRENT_LIMIT_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(
        CONF_BUS_VOLTAGE, CONF_SHUNT_VOLTAGE, CONF_CURRENT, CONF_POWER
    ),
    validate_alert_limits,
)


def adc_times_us(config: ConfigType) -> tuple[int, int]:
    """Return the bus and shunt conversion times in microseconds."""
    adc_time = config[CONF_ADC_TIME]
    if isinstance(adc_time, dict):
        return adc_time[CONF_BUS_VOLTAGE], adc_time[CONF_SHUNT_VOLTAGE]
    return adc_time, adc_time


def single_shot_wait_ms(config: ConfigType) -> int:
    """Time one conversion of every enabled channel takes, plus headroom."""
    bus_us, shunt_us = adc_times_us(config)
    channels = sum(1 for channel in CHANNELS if channel in config)
    total_us = (bus_us + shunt_us) * config[CONF_ADC_AVERAGING] * channels
    return -(-total_us // 1000) + SINGLE_SHOT_MARGIN_MS


def validate_single_shot_interval(config: ConfigType) -> ConfigType:
    if config[CONF_CONTINUOUS_MODE]:
        return config
    wait_ms = single_shot_wait_ms(config)
    if config[CONF_UPDATE_INTERVAL].total_milliseconds <= wait_ms:
        raise cv.Invalid(
            f"A single-shot conversion of all channels takes {wait_ms} ms, which does "
            f"not fit in the update_interval; lower {CONF_ADC_AVERAGING} or "
            f"{CONF_ADC_TIME}, or raise the interval",
            path=[CONF_UPDATE_INTERVAL],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(INA3221Component),
            cv.Optional(CONF_CHANNEL_1): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_2): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_3): INA3221_CHANNEL_SCHEMA,
            cv.Optional(CONF_CONTINUOUS_MODE, default=True): cv.boolean,
            cv.Optional(CONF_ADC_AVERAGING, default=1): cv.one_of(
                *ADC_AVERAGING, int=True
            ),
            cv.Optional(CONF_ADC_TIME, default="8244us"): cv.Any(
                validate_adc_time,
                cv.Schema(
                    {
                        cv.Required(CONF_BUS_VOLTAGE): validate_adc_time,
                        cv.Required(CONF_SHUNT_VOLTAGE): validate_adc_time,
                    }
                ),
            ),
            cv.Optional(CONF_SUM_SHUNT_VOLTAGE): VOLTAGE_SCHEMA,
            cv.Optional(CONF_SUM_CURRENT): CURRENT_SCHEMA,
            cv.Optional(CONF_SUM_POWER): POWER_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x40)),
    cv.has_at_least_one_key(*CHANNELS),
    validate_single_shot_interval,
)


def alert_limit_register(current_a: float, shunt_ohm: float) -> int:
    return round(current_a * shunt_ohm / ALERT_LIMIT_LSB_V) << 3


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    bus_us, shunt_us = adc_times_us(config)
    register = (
        ADC_AVERAGING[config[CONF_ADC_AVERAGING]] << ADC_AVERAGING_SHIFT
        | ADC_TIMES_US[bus_us] << ADC_TIME_BUS_SHIFT
        | ADC_TIMES_US[shunt_us] << ADC_TIME_SHUNT_SHIFT
        | (MODE_CONTINUOUS if config[CONF_CONTINUOUS_MODE] else MODE_SINGLE_SHOT)
    )
    for i, channel in enumerate(CHANNELS):
        if (conf := config.get(channel)) is None:
            continue
        register |= CHANNEL_ENABLE_BITS[i]
        shunt = conf[CONF_SHUNT_RESISTANCE]
        cg.add(var.set_shunt_resistance(i, shunt))
        if CONF_BUS_VOLTAGE in conf:
            sens = await sensor.new_sensor(conf[CONF_BUS_VOLTAGE])
            cg.add(var.set_bus_voltage_sensor(i, sens))
        if CONF_SHUNT_VOLTAGE in conf:
            sens = await sensor.new_sensor(conf[CONF_SHUNT_VOLTAGE])
            cg.add(var.set_shunt_voltage_sensor(i, sens))
        if CONF_CURRENT in conf:
            sens = await sensor.new_sensor(conf[CONF_CURRENT])
            cg.add(var.set_current_sensor(i, sens))
        if CONF_POWER in conf:
            sens = await sensor.new_sensor(conf[CONF_POWER])
            cg.add(var.set_power_sensor(i, sens))
        for key, setter in (
            (CONF_WARNING_CURRENT_LIMIT, var.set_warning_limit),
            (CONF_CRITICAL_CURRENT_LIMIT, var.set_critical_limit),
        ):
            if (limit := conf.get(key)) is not None:
                cg.add_define("USE_INA3221_ALERT_LIMITS")
                cg.add(setter(i, alert_limit_register(limit, shunt)))
    cg.add(var.set_config_register(register))
    if not config[CONF_CONTINUOUS_MODE]:
        cg.add(var.set_single_shot_wait_ms(single_shot_wait_ms(config)))

    if any(
        key in config
        for key in (CONF_SUM_SHUNT_VOLTAGE, CONF_SUM_CURRENT, CONF_SUM_POWER)
    ):
        cg.add_define("USE_INA3221_SUMMATION")
        sensors = sensor.sub_sensors(config)
        await sensors(CONF_SUM_SHUNT_VOLTAGE, var.set_sum_shunt_voltage_sensor)
        await sensors(CONF_SUM_CURRENT, var.set_sum_current_sensor)
        await sensors(CONF_SUM_POWER, var.set_sum_power_sensor)
