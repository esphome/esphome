import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
)
from esphome.types import ConfigType

from .. import CONF_BQ25820_ID, BQ25820BaseSchema

DEPENDENCIES = ["bq25820"]

CONF_CHARGING = "charging"
CONF_CHARGED = "charged"
CONF_LOW_BATTERY = "low_battery"
CONF_PG = "pg"
CONF_REVERSE = "reverse"
CONF_VAC_UNDERVOLTAGE = "vac_undervoltage"
CONF_VAC_OVERVOLTAGE = "vac_overvoltage"
CONF_IBAT_OVERCURRENT = "ibat_overcurrent"
CONF_VBAT_OVERVOLTAGE = "vbat_overvoltage"
CONF_THERMAL_SHUTDOWN = "thermal_shutdown"
CONF_CHARGE_SAFETY_TIMER = "charge_safety_timer"
CONF_DRV_SUPPLY_INVALID = "drv_supply_invalid"


_FAULT_KEYS = (
    CONF_VAC_UNDERVOLTAGE,
    CONF_VAC_OVERVOLTAGE,
    CONF_IBAT_OVERCURRENT,
    CONF_VBAT_OVERVOLTAGE,
    CONF_THERMAL_SHUTDOWN,
    CONF_CHARGE_SAFETY_TIMER,
    CONF_DRV_SUPPLY_INVALID,
)
_FAULT_SCHEMA = binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_PROBLEM)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY_CHARGING,
        ),
        cv.Optional(CONF_CHARGED): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_LOW_BATTERY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY,
        ),
        cv.Optional(CONF_PG): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_REVERSE): binary_sensor.binary_sensor_schema(),
        **{cv.Optional(key): _FAULT_SCHEMA for key in _FAULT_KEYS},
    }
).extend(BQ25820BaseSchema)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_CHARGING, hub.set_charging_binary_sensor)
    await binary_sensors(CONF_CHARGED, hub.set_charged_binary_sensor)
    await binary_sensors(CONF_LOW_BATTERY, hub.set_low_battery_binary_sensor)
    await binary_sensors(CONF_PG, hub.set_pg_binary_sensor)
    await binary_sensors(CONF_REVERSE, hub.set_reverse_binary_sensor)
    await binary_sensors(CONF_VAC_UNDERVOLTAGE, hub.set_vac_undervoltage_binary_sensor)
    await binary_sensors(CONF_VAC_OVERVOLTAGE, hub.set_vac_overvoltage_binary_sensor)
    await binary_sensors(CONF_IBAT_OVERCURRENT, hub.set_ibat_overcurrent_binary_sensor)
    await binary_sensors(CONF_VBAT_OVERVOLTAGE, hub.set_vbat_overvoltage_binary_sensor)
    await binary_sensors(CONF_THERMAL_SHUTDOWN, hub.set_thermal_shutdown_binary_sensor)
    await binary_sensors(
        CONF_CHARGE_SAFETY_TIMER, hub.set_charge_safety_timer_binary_sensor
    )
    await binary_sensors(
        CONF_DRV_SUPPLY_INVALID, hub.set_drv_supply_invalid_binary_sensor
    )
