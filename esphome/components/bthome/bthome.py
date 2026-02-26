from dataclasses import dataclass
from enum import Enum, auto

from esphome import const
import esphome.codegen as cg

bthome_ns = cg.esphome_ns.namespace("bthome")
bthome_object_types = bthome_ns.enum("BTHomeObjectType", True)


class OTKind(Enum):
    SENSOR = auto()
    BINARY_SENSOR = auto()
    TEXT_SENSOR = auto()


@dataclass
class OT:
    """BTHome object type descriptor with sensor display defaults."""

    object_id: int
    unit: str = const.UNIT_EMPTY
    device_class: str | None = None
    state_class: str = const.STATE_CLASS_MEASUREMENT
    kind: OTKind = OTKind.SENSOR


_TI = const.STATE_CLASS_TOTAL_INCREASING
_MA = const.STATE_CLASS_MEASUREMENT_ANGLE

BTHOME_OBJECT_TYPES: dict[str, OT] = {
    "PACKET_ID": OT(0x00),
    "BATTERY_PCT": OT(0x01, const.UNIT_PERCENT, const.DEVICE_CLASS_BATTERY),
    "TEMPERATURE_C_E2": OT(0x02, const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    "HUMIDITY_PCT_E2": OT(0x03, const.UNIT_PERCENT, const.DEVICE_CLASS_HUMIDITY),
    "PRESSURE_HPA_E2": OT(
        0x04, const.UNIT_HECTOPASCAL, const.DEVICE_CLASS_ATMOSPHERIC_PRESSURE
    ),
    "ILLUMINANCE_LX_E2": OT(0x05, const.UNIT_LUX, const.DEVICE_CLASS_ILLUMINANCE),
    "MASS_KG_E2": OT(0x06, const.UNIT_KILOGRAM, const.DEVICE_CLASS_WEIGHT),
    "MASS_LB_E2": OT(0x07, const.UNIT_POUND, const.DEVICE_CLASS_WEIGHT),
    "DEWPOINT_C_E2": OT(0x08, const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    "COUNT_U8": OT(0x09),
    "ENERGY_KWH_E3": OT(
        0x0A, const.UNIT_KILOWATT_HOURS, const.DEVICE_CLASS_ENERGY, state_class=_TI
    ),
    "POWER_W_E2": OT(0x0B, const.UNIT_WATT, const.DEVICE_CLASS_POWER),
    "VOLTAGE_V_E3": OT(0x0C, const.UNIT_VOLT, const.DEVICE_CLASS_VOLTAGE),
    "PM25_UGM3": OT(
        0x0D, const.UNIT_MICROGRAMS_PER_CUBIC_METER, const.DEVICE_CLASS_PM25
    ),
    "PM10_UGM3": OT(
        0x0E, const.UNIT_MICROGRAMS_PER_CUBIC_METER, const.DEVICE_CLASS_PM10
    ),
    "GENERIC_BOOLEAN": OT(0x0F, kind=OTKind.BINARY_SENSOR),
    "POWER_ON": OT(0x10, kind=OTKind.BINARY_SENSOR),
    "OPENING_OPEN": OT(
        0x11, device_class=const.DEVICE_CLASS_OPENING, kind=OTKind.BINARY_SENSOR
    ),
    "CO2_PPM": OT(
        0x12, const.UNIT_PARTS_PER_MILLION, const.DEVICE_CLASS_CARBON_DIOXIDE
    ),
    "TVOC_UGM3": OT(
        0x13,
        const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        const.DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ),
    "MOISTURE_PCT_E2": OT(0x14, const.UNIT_PERCENT, const.DEVICE_CLASS_MOISTURE),
    "BATTERY_LOW": OT(
        0x15, device_class=const.DEVICE_CLASS_BATTERY, kind=OTKind.BINARY_SENSOR
    ),
    "BATTERY_CHARGING": OT(
        0x16,
        device_class=const.DEVICE_CLASS_BATTERY_CHARGING,
        kind=OTKind.BINARY_SENSOR,
    ),
    "CO_DETECTED": OT(
        0x17,
        device_class=const.DEVICE_CLASS_CARBON_MONOXIDE,
        kind=OTKind.BINARY_SENSOR,
    ),
    "COLD_DETECTED": OT(
        0x18, device_class=const.DEVICE_CLASS_COLD, kind=OTKind.BINARY_SENSOR
    ),
    "CONNECTIVITY_CONNECTED": OT(
        0x19, device_class=const.DEVICE_CLASS_CONNECTIVITY, kind=OTKind.BINARY_SENSOR
    ),
    "DOOR_OPEN": OT(
        0x1A, device_class=const.DEVICE_CLASS_DOOR, kind=OTKind.BINARY_SENSOR
    ),
    "GARAGE_DOOR_OPEN": OT(
        0x1B, device_class=const.DEVICE_CLASS_OPENING, kind=OTKind.BINARY_SENSOR
    ),
    "GAS_DETECTED": OT(
        0x1C, device_class=const.DEVICE_CLASS_GAS, kind=OTKind.BINARY_SENSOR
    ),
    "HEAT_DETECTED": OT(
        0x1D, device_class=const.DEVICE_CLASS_HEAT, kind=OTKind.BINARY_SENSOR
    ),
    "LIGHT_DETECTED": OT(
        0x1E, device_class=const.DEVICE_CLASS_LIGHT, kind=OTKind.BINARY_SENSOR
    ),
    "LOCK_UNLOCKED": OT(
        0x1F, device_class=const.DEVICE_CLASS_LOCK, kind=OTKind.BINARY_SENSOR
    ),
    "MOISTURE_WET": OT(
        0x20, device_class=const.DEVICE_CLASS_MOISTURE, kind=OTKind.BINARY_SENSOR
    ),
    "MOTION_DETECTED": OT(
        0x21, device_class=const.DEVICE_CLASS_MOTION, kind=OTKind.BINARY_SENSOR
    ),
    "MOVING_ACTIVE": OT(
        0x22, device_class=const.DEVICE_CLASS_MOVING, kind=OTKind.BINARY_SENSOR
    ),
    "OCCUPANCY_DETECTED": OT(
        0x23, device_class=const.DEVICE_CLASS_OCCUPANCY, kind=OTKind.BINARY_SENSOR
    ),
    "PLUG_PLUGGED_IN": OT(
        0x24, device_class=const.DEVICE_CLASS_PLUG, kind=OTKind.BINARY_SENSOR
    ),
    "PRESENCE_HOME": OT(
        0x25, device_class=const.DEVICE_CLASS_PRESENCE, kind=OTKind.BINARY_SENSOR
    ),
    "PROBLEM_DETECTED": OT(
        0x26, device_class=const.DEVICE_CLASS_PROBLEM, kind=OTKind.BINARY_SENSOR
    ),
    "RUNNING_ACTIVE": OT(
        0x27, device_class=const.DEVICE_CLASS_RUNNING, kind=OTKind.BINARY_SENSOR
    ),
    "SAFETY_SAFE": OT(
        0x28, device_class=const.DEVICE_CLASS_SAFETY, kind=OTKind.BINARY_SENSOR
    ),
    "SMOKE_DETECTED": OT(
        0x29, device_class=const.DEVICE_CLASS_SMOKE, kind=OTKind.BINARY_SENSOR
    ),
    "SOUND_DETECTED": OT(
        0x2A, device_class=const.DEVICE_CLASS_SOUND, kind=OTKind.BINARY_SENSOR
    ),
    "TAMPER_ACTIVE": OT(
        0x2B, device_class=const.DEVICE_CLASS_TAMPER, kind=OTKind.BINARY_SENSOR
    ),
    "VIBRATION_DETECTED": OT(
        0x2C, device_class=const.DEVICE_CLASS_VIBRATION, kind=OTKind.BINARY_SENSOR
    ),
    "WINDOW_OPEN": OT(
        0x2D, device_class=const.DEVICE_CLASS_WINDOW, kind=OTKind.BINARY_SENSOR
    ),
    "HUMIDITY_PCT_U8": OT(0x2E, const.UNIT_PERCENT, const.DEVICE_CLASS_HUMIDITY),
    "MOISTURE_PCT_U8": OT(0x2F, const.UNIT_PERCENT, const.DEVICE_CLASS_MOISTURE),
    "COUNT_U16": OT(0x3D),
    "COUNT_U32": OT(0x3E),
    "ROTATION_DEG_E1": OT(0x3F, const.UNIT_DEGREES, state_class=_MA),
    "DISTANCE_MM": OT(0x40, const.UNIT_MILLIMETER, const.DEVICE_CLASS_DISTANCE),
    "DISTANCE_M_E1": OT(0x41, const.UNIT_METER, const.DEVICE_CLASS_DISTANCE),
    "DURATION_S_E3": OT(0x42, const.UNIT_SECOND, const.DEVICE_CLASS_DURATION),
    "CURRENT_A_E3": OT(0x43, const.UNIT_AMPERE, const.DEVICE_CLASS_CURRENT),
    "SPEED_MS_E2": OT(0x44, const.UNIT_METER_PER_SECOND, const.DEVICE_CLASS_SPEED),
    "TEMPERATURE_C_E1": OT(0x45, const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    "UV_INDEX_E1": OT(0x46),
    "VOLUME_L_E1": OT(0x47, const.UNIT_LITRE, const.DEVICE_CLASS_VOLUME),
    "VOLUME_ML": OT(0x48, const.UNIT_MILLILITRE, const.DEVICE_CLASS_VOLUME),
    "VOLUME_FLOW_M3HR_E3": OT(
        0x49, const.UNIT_CUBIC_METER_PER_HOUR, const.DEVICE_CLASS_VOLUME_FLOW_RATE
    ),
    "VOLTAGE_V_E1": OT(0x4A, const.UNIT_VOLT, const.DEVICE_CLASS_VOLTAGE),
    "GAS_M3_U24_E3": OT(
        0x4B, const.UNIT_CUBIC_METER, const.DEVICE_CLASS_GAS, state_class=_TI
    ),
    "GAS_M3_U32_E3": OT(
        0x4C, const.UNIT_CUBIC_METER, const.DEVICE_CLASS_GAS, state_class=_TI
    ),
    "ENERGY_KWH_U32_E3": OT(
        0x4D, const.UNIT_KILOWATT_HOURS, const.DEVICE_CLASS_ENERGY, state_class=_TI
    ),
    "VOLUME_L_U32_E3": OT(0x4E, const.UNIT_LITRE, const.DEVICE_CLASS_VOLUME),
    "WATER_L_E3": OT(0x4F, const.UNIT_LITRE, const.DEVICE_CLASS_WATER, state_class=_TI),
    "TIMESTAMP": OT(0x50, device_class=const.DEVICE_CLASS_TIMESTAMP),
    "ACCELERATION_MSS_E3": OT(0x51, const.UNIT_METER_PER_SECOND_SQUARED),
    "GYROSCOPE_DEGS_E3": OT(0x52, const.UNIT_DEGREE_PER_SECOND),
    "TEXT": OT(0x53, kind=OTKind.TEXT_SENSOR),
    "RAW": OT(0x54, kind=OTKind.TEXT_SENSOR),
    "VOLUME_STORAGE_L_E3": OT(
        0x55, const.UNIT_LITRE, const.DEVICE_CLASS_VOLUME_STORAGE
    ),
    "CONDUCTIVITY_USCM": OT(
        0x56, const.UNIT_MICROSIEMENS_PER_CENTIMETER, const.DEVICE_CLASS_CONDUCTIVITY
    ),
    "TEMPERATURE_C_I8": OT(0x57, const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    "TEMPERATURE_C_I8_0_35": OT(
        0x58, const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE
    ),
    "COUNT_I8": OT(0x59),
    "COUNT_I16": OT(0x5A),
    "COUNT_I32": OT(0x5B),
    "POWER_W_I32_E2": OT(0x5C, const.UNIT_WATT, const.DEVICE_CLASS_POWER),
    "CURRENT_A_I16_E3": OT(0x5D, const.UNIT_AMPERE, const.DEVICE_CLASS_CURRENT),
    "DIRECTION_DEG_E2": OT(
        0x5E, const.UNIT_DEGREES, const.DEVICE_CLASS_WIND_DIRECTION, state_class=_MA
    ),
    "PRECIPITATION_MM_E1": OT(
        0x5F, const.UNIT_MILLIMETER, const.DEVICE_CLASS_PRECIPITATION
    ),
    "CHANNEL": OT(0x60),
    "ROTATIONAL_SPEED_RPM": OT(0x61, const.UNIT_REVOLUTIONS_PER_MINUTE),
    "SPEED_MS_I32_E6": OT(0x62, const.UNIT_METER_PER_SECOND, const.DEVICE_CLASS_SPEED),
    "ACCELERATION_MSS_I32_E6": OT(0x63, const.UNIT_METER_PER_SECOND_SQUARED),
}
