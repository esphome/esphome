import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_CURRENT,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_MINUTE,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

bq27220_ns = cg.esphome_ns.namespace("bq27220")
BQ27220Component = bq27220_ns.class_(
    "BQ27220Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_REMAINING_CAPACITY = "remaining_capacity"
CONF_FULL_CHARGE_CAPACITY = "full_charge_capacity"
CONF_TIME_TO_EMPTY = "time_to_empty"
CONF_STATE_OF_HEALTH = "state_of_health"
UNIT_MILLIAMPERE_HOUR = "mAh"

# Other SLUUBD4A standard commands are intentionally not exposed to keep the
# component focused; they can be added later if there is demand: TimeToFull
# (0x18), CycleCount (0x2A), BatteryStatus (0x0A), AveragePower (0x24),
# DesignCapacity (0x3C).

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ27220Component),
            cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CURRENT): sensor.sensor_schema(
                unit_of_measurement=UNIT_AMPERE,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_CURRENT,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_REMAINING_CAPACITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE_HOUR,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_BATTERY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_FULL_CHARGE_CAPACITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIAMPERE_HOUR,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_BATTERY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TIME_TO_EMPTY): sensor.sensor_schema(
                unit_of_measurement=UNIT_MINUTE,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STATE_OF_HEALTH): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_BATTERY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x55))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE, var.set_voltage_sensor)
    await sensors(CONF_CURRENT, var.set_current_sensor)
    await sensors(CONF_BATTERY_LEVEL, var.set_battery_level_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_REMAINING_CAPACITY, var.set_remaining_capacity_sensor)
    await sensors(CONF_FULL_CHARGE_CAPACITY, var.set_full_charge_capacity_sensor)
    await sensors(CONF_TIME_TO_EMPTY, var.set_time_to_empty_sensor)
    await sensors(CONF_STATE_OF_HEALTH, var.set_state_of_health_sensor)
