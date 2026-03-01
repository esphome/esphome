"""Battery voltage and level sensor for LILYGO T5 4.7" Plus."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    ICON_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from .. import lilygo_t5_47_plus_ns

CODEOWNERS = ["@hbast"]
DEPENDENCIES = ["esp32", "psram"]

CONF_MIN_VOLTAGE = "min_voltage"
CONF_MAX_VOLTAGE = "max_voltage"
CONF_VOLTAGE_DIVIDER = "voltage_divider"

LilygoT547PlusBattery = lilygo_t5_47_plus_ns.class_(
    "LilygoT547PlusBattery",
    cg.PollingComponent,
)

# Schema for the voltage sensor
BATTERY_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
    icon=ICON_BATTERY,
)

# Schema for the percentage sensor
BATTERY_LEVEL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_BATTERY,
    state_class=STATE_CLASS_MEASUREMENT,
    icon=ICON_BATTERY,
)

_SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LilygoT547PlusBattery),
        cv.Optional(CONF_BATTERY_VOLTAGE): BATTERY_VOLTAGE_SCHEMA,
        cv.Optional(CONF_BATTERY_LEVEL): BATTERY_LEVEL_SCHEMA,
        cv.Optional(CONF_MIN_VOLTAGE, default=3.0): cv.float_range(min=2.5, max=3.5),
        cv.Optional(CONF_MAX_VOLTAGE, default=4.2): cv.float_range(min=3.7, max=4.5),
        cv.Optional(CONF_VOLTAGE_DIVIDER, default=2.0): cv.float_range(
            min=1.0, max=10.0
        ),
    }
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.All(
    _SENSOR_SCHEMA,
    cv.has_at_least_one_key(CONF_BATTERY_VOLTAGE, CONF_BATTERY_LEVEL),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_min_voltage(config[CONF_MIN_VOLTAGE]))
    cg.add(var.set_max_voltage(config[CONF_MAX_VOLTAGE]))
    cg.add(var.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))

    if battery_voltage_config := config.get(CONF_BATTERY_VOLTAGE):
        sens = await sensor.new_sensor(battery_voltage_config)
        cg.add(var.set_battery_voltage_sensor(sens))

    if battery_level_config := config.get(CONF_BATTERY_LEVEL):
        sens = await sensor.new_sensor(battery_level_config)
        cg.add(var.set_battery_level_sensor(sens))
