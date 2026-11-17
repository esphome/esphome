import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_ERROR_CODE
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
    UNIT_WATT,
)
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner, MideaEnergyFormat

CODEOWNERS = ["@dudanov"]

CONF_COMPRESSOR_CURRENT = "compressor_current"
CONF_COMPRESSOR_FREQUENCY = "compressor_frequency"
CONF_COMPRESSOR_TARGET_FREQUENCY = "compressor_target_frequency"
CONF_COMPRESSOR_VOLTAGE = "compressor_voltage"
CONF_CURRENT_ENERGY = "current_energy"
CONF_DISCHARGE_TEMPERATURE = "discharge_temperature"
CONF_ENERGY_FORMAT = "energy_format"
CONF_HORIZONTAL_LOUVER_ANGLE = "horizontal_louver_angle"
CONF_INDOOR_COIL_TEMPERATURE = "indoor_coil_temperature"
CONF_INDOOR_FAN_SPEED = "indoor_fan_speed"
CONF_INDOOR_FAN_TARGET_SPEED = "indoor_fan_target_speed"
CONF_INDOOR_HUMIDITY = "indoor_humidity"
CONF_OUTDOOR_COIL_TEMPERATURE = "outdoor_coil_temperature"
CONF_OUTDOOR_FAN_SPEED = "outdoor_fan_speed"
CONF_OUTDOOR_UNIT_POWER = "outdoor_unit_power"
CONF_REAL_TIME_POWER = "real_time_power"
CONF_TOTAL_ENERGY = "total_energy"
CONF_VERTICAL_LOUVER_ANGLE = "vertical_louver_angle"

ENERGY_FORMATS = MideaEnergyFormat.options(
    "BCD", "BINARY", prefix="MIDEA_ENERGY_FORMAT_"
)

ICON_ALERT = "mdi:alert-circle-outline"
ICON_ANGLE = "mdi:angle-acute"


def _measurement(unit, device_class=cv.UNDEFINED, accuracy=0, icon=cv.UNDEFINED):
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        device_class=device_class,
        accuracy_decimals=accuracy,
        icon=icon,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _diagnostic(unit, device_class=cv.UNDEFINED, accuracy=0, icon=cv.UNDEFINED):
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        device_class=device_class,
        accuracy_decimals=accuracy,
        icon=icon,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


def _energy():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_KILOWATT_HOURS,
        device_class=DEVICE_CLASS_ENERGY,
        accuracy_decimals=2,
        state_class=STATE_CLASS_TOTAL_INCREASING,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # Group 4 energy values come as BCD on most units and as binary on some
        cv.Optional(CONF_ENERGY_FORMAT, default="BCD"): cv.enum(
            ENERGY_FORMATS, upper=True
        ),
        # 0xC0 status
        cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(
            icon=ICON_ALERT,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        # Group 1
        cv.Optional(CONF_COMPRESSOR_FREQUENCY): _diagnostic(
            UNIT_HERTZ, DEVICE_CLASS_FREQUENCY
        ),
        cv.Optional(CONF_COMPRESSOR_TARGET_FREQUENCY): _diagnostic(
            UNIT_HERTZ, DEVICE_CLASS_FREQUENCY
        ),
        cv.Optional(CONF_COMPRESSOR_CURRENT): _diagnostic(
            UNIT_AMPERE, DEVICE_CLASS_CURRENT
        ),
        cv.Optional(CONF_COMPRESSOR_VOLTAGE): _diagnostic(
            UNIT_VOLT, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional(CONF_INDOOR_COIL_TEMPERATURE): _diagnostic(
            UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1
        ),
        cv.Optional(CONF_OUTDOOR_COIL_TEMPERATURE): _diagnostic(
            UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, 1
        ),
        cv.Optional(CONF_DISCHARGE_TEMPERATURE): _diagnostic(
            UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE
        ),
        # Group 2
        cv.Optional(CONF_INDOOR_FAN_SPEED): _diagnostic(
            UNIT_REVOLUTIONS_PER_MINUTE, icon=ICON_FAN
        ),
        cv.Optional(CONF_INDOOR_FAN_TARGET_SPEED): _diagnostic(
            UNIT_REVOLUTIONS_PER_MINUTE, icon=ICON_FAN
        ),
        # Group 4
        cv.Optional(CONF_TOTAL_ENERGY): _energy(),
        cv.Optional(CONF_CURRENT_ENERGY): _energy(),
        cv.Optional(CONF_REAL_TIME_POWER): _measurement(
            UNIT_WATT, DEVICE_CLASS_POWER, 1
        ),
        # Group 5
        cv.Optional(CONF_INDOOR_HUMIDITY): _measurement(
            UNIT_PERCENT, DEVICE_CLASS_HUMIDITY
        ),
        cv.Optional(CONF_OUTDOOR_FAN_SPEED): _diagnostic(
            UNIT_REVOLUTIONS_PER_MINUTE, icon=ICON_FAN
        ),
        # Group 7
        cv.Optional(CONF_OUTDOOR_UNIT_POWER): _measurement(
            UNIT_WATT, DEVICE_CLASS_POWER
        ),
        # Group 11
        cv.Optional(CONF_HORIZONTAL_LOUVER_ANGLE): _diagnostic(
            UNIT_DEGREES, icon=ICON_ANGLE
        ),
        cv.Optional(CONF_VERTICAL_LOUVER_ANGLE): _diagnostic(
            UNIT_DEGREES, icon=ICON_ANGLE
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    cg.add(hub.set_energy_format(config[CONF_ENERGY_FORMAT]))
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ERROR_CODE, hub.set_error_code_sensor)
    await sensors(CONF_COMPRESSOR_FREQUENCY, hub.set_compressor_frequency_sensor)
    await sensors(
        CONF_COMPRESSOR_TARGET_FREQUENCY, hub.set_compressor_target_frequency_sensor
    )
    await sensors(CONF_COMPRESSOR_CURRENT, hub.set_compressor_current_sensor)
    await sensors(CONF_COMPRESSOR_VOLTAGE, hub.set_compressor_voltage_sensor)
    await sensors(CONF_INDOOR_COIL_TEMPERATURE, hub.set_indoor_coil_temperature_sensor)
    await sensors(
        CONF_OUTDOOR_COIL_TEMPERATURE, hub.set_outdoor_coil_temperature_sensor
    )
    await sensors(CONF_DISCHARGE_TEMPERATURE, hub.set_discharge_temperature_sensor)
    await sensors(CONF_INDOOR_FAN_SPEED, hub.set_indoor_fan_speed_sensor)
    await sensors(CONF_INDOOR_FAN_TARGET_SPEED, hub.set_indoor_fan_target_speed_sensor)
    await sensors(CONF_TOTAL_ENERGY, hub.set_total_energy_sensor)
    await sensors(CONF_CURRENT_ENERGY, hub.set_current_energy_sensor)
    await sensors(CONF_REAL_TIME_POWER, hub.set_real_time_power_sensor)
    await sensors(CONF_INDOOR_HUMIDITY, hub.set_indoor_humidity_sensor)
    await sensors(CONF_OUTDOOR_FAN_SPEED, hub.set_outdoor_fan_speed_sensor)
    await sensors(CONF_OUTDOOR_UNIT_POWER, hub.set_outdoor_unit_power_sensor)
    await sensors(CONF_HORIZONTAL_LOUVER_ANGLE, hub.set_horizontal_louver_angle_sensor)
    await sensors(CONF_VERTICAL_LOUVER_ANGLE, hub.set_vertical_louver_angle_sensor)
