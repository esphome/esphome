"""TMP102 temperature sensor and optional hardware configuration."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from esphome.types import ConfigType

from . import (
    ALERT_POLARITIES,
    CONF_ALERT,
    CONF_ALERT_POLARITY,
    CONF_CONVERSION_RATE,
    CONF_EXTENDED_MODE,
    CONF_FAULT_QUEUE,
    CONF_ONE_SHOT_MODE,
    CONF_TEMPERATURE_HIGH,
    CONF_TEMPERATURE_LOW,
    CONF_THERMOSTAT_MODE,
    CONF_THRESHOLD_STATUS,
    CONVERSION_RATES,
    THERMOSTAT_MODES,
    TMP102Component,
    validate_parent_thresholds,
)

CODEOWNERS = ["@timsavage"]
DEPENDENCIES = ["i2c"]

# No defaults here: absence of all advanced options preserves upstream's read-only behavior.
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        TMP102Component,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
    .extend(
        {
            cv.Optional(CONF_EXTENDED_MODE): cv.boolean,
            cv.Optional(CONF_CONVERSION_RATE): cv.enum(CONVERSION_RATES),
            cv.Optional(CONF_ONE_SHOT_MODE): cv.boolean,
            cv.Optional(CONF_ALERT_POLARITY): cv.enum(ALERT_POLARITIES),
            cv.Optional(CONF_THERMOSTAT_MODE): cv.enum(THERMOSTAT_MODES),
            cv.Optional(CONF_FAULT_QUEUE): cv.one_of(1, 2, 4, 6, int=True),
            cv.Optional(CONF_TEMPERATURE_HIGH): cv.temperature,
            cv.Optional(CONF_TEMPERATURE_LOW): cv.temperature,
            cv.Optional(CONF_ALERT): cv.invalid(
                "Move alert to binary_sensor with platform: tmp102 and tmp102_id"
            ),
            cv.Optional(CONF_THRESHOLD_STATUS): cv.invalid(
                "Move threshold_status to text_sensor with platform: tmp102 and tmp102_id"
            ),
        }
    )
)

FINAL_VALIDATE_SCHEMA = validate_parent_thresholds


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    for key, setter in (
        (CONF_EXTENDED_MODE, var.set_extended_mode),
        (CONF_CONVERSION_RATE, var.set_conversion_rate),
        (CONF_ONE_SHOT_MODE, var.set_one_shot_mode),
        (CONF_ALERT_POLARITY, var.set_alert_polarity),
        (CONF_THERMOSTAT_MODE, var.set_thermostat_mode),
        (CONF_FAULT_QUEUE, var.set_fault_queue),
        (CONF_TEMPERATURE_HIGH, var.set_temperature_high),
        (CONF_TEMPERATURE_LOW, var.set_temperature_low),
    ):
        if key in config:
            cg.add(setter(config[key]))
            cg.add(var.set_configure(True))
