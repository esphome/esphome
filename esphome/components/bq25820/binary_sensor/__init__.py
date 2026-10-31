import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_BATTERY, DEVICE_CLASS_PLUG, DEVICE_CLASS_POWER

from .. import CONF_BQ25820_ID, BQ25820BaseSchema, bq25820_ns

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

BQ25820ChargingBinarySensor = bq25820_ns.class_(
    "BQ25820ChargingBinarySensor", binary_sensor.BinarySensor
)
BQ25820ChargedBinarySensor = bq25820_ns.class_(
    "BQ25820ChargedBinarySensor", binary_sensor.BinarySensor
)
BQ25820LowBatteryBinarySensor = bq25820_ns.class_(
    "BQ25820LowBatteryBinarySensor", binary_sensor.BinarySensor
)
BQ25820PgBinarySensor = bq25820_ns.class_(
    "BQ25820PgBinarySensor", binary_sensor.BinarySensor
)
BQ25820ReverseBinarySensor = bq25820_ns.class_(
    "BQ25820ReverseBinarySensor", binary_sensor.BinarySensor
)
BQ25820VacUvBinarySensor = bq25820_ns.class_(
    "BQ25820VacUvBinarySensor", binary_sensor.BinarySensor
)
BQ25820VacOvBinarySensor = bq25820_ns.class_(
    "BQ25820VacOvBinarySensor", binary_sensor.BinarySensor
)
BQ25820IbatOcBinarySensor = bq25820_ns.class_(
    "BQ25820IbatOcBinarySensor", binary_sensor.BinarySensor
)
BQ25820VbatOvBinarySensor = bq25820_ns.class_(
    "BQ25820VbatOvBinarySensor", binary_sensor.BinarySensor
)
BQ25820ThermalShutdownBinarySensor = bq25820_ns.class_(
    "BQ25820ThermalShutdownBinarySensor", binary_sensor.BinarySensor
)
BQ25820ChargeSafetyTimerBinarySensor = bq25820_ns.class_(
    "BQ25820ChargeSafetyTimerBinarySensor", binary_sensor.BinarySensor
)
BQ25820DrvSupplyInvalidBinarySensor = bq25820_ns.class_(
    "BQ25820DrvSupplyInvalidBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            BQ25820ChargingBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGED): binary_sensor.binary_sensor_schema(
            BQ25820ChargedBinarySensor,
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_LOW_BATTERY): binary_sensor.binary_sensor_schema(
            BQ25820LowBatteryBinarySensor,
            device_class=DEVICE_CLASS_BATTERY,
        ),
        cv.Optional(CONF_PG): binary_sensor.binary_sensor_schema(
            BQ25820PgBinarySensor,
            device_class=DEVICE_CLASS_PLUG,
        ),
        cv.Optional(CONF_REVERSE): binary_sensor.binary_sensor_schema(
            BQ25820ReverseBinarySensor,
        ),
        cv.Optional(CONF_VAC_UNDERVOLTAGE): binary_sensor.binary_sensor_schema(
            BQ25820VacUvBinarySensor,
        ),
        cv.Optional(CONF_VAC_OVERVOLTAGE): binary_sensor.binary_sensor_schema(
            BQ25820VacOvBinarySensor,
        ),
        cv.Optional(CONF_IBAT_OVERCURRENT): binary_sensor.binary_sensor_schema(
            BQ25820IbatOcBinarySensor,
        ),
        cv.Optional(CONF_VBAT_OVERVOLTAGE): binary_sensor.binary_sensor_schema(
            BQ25820VbatOvBinarySensor,
        ),
        cv.Optional(CONF_THERMAL_SHUTDOWN): binary_sensor.binary_sensor_schema(
            BQ25820ThermalShutdownBinarySensor,
        ),
        cv.Optional(CONF_CHARGE_SAFETY_TIMER): binary_sensor.binary_sensor_schema(
            BQ25820ChargeSafetyTimerBinarySensor,
        ),
        cv.Optional(CONF_DRV_SUPPLY_INVALID): binary_sensor.binary_sensor_schema(
            BQ25820DrvSupplyInvalidBinarySensor,
        ),
    }
).extend(BQ25820BaseSchema)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25820_ID])
    for key in (
        CONF_CHARGING,
        CONF_CHARGED,
        CONF_LOW_BATTERY,
        CONF_PG,
        CONF_REVERSE,
        CONF_VAC_UNDERVOLTAGE,
        CONF_VAC_OVERVOLTAGE,
        CONF_IBAT_OVERCURRENT,
        CONF_VBAT_OVERVOLTAGE,
        CONF_THERMAL_SHUTDOWN,
        CONF_CHARGE_SAFETY_TIMER,
        CONF_DRV_SUPPLY_INVALID,
    ):
        if (conf := config.get(key)) is not None:
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(parent.add_listener(sens))
