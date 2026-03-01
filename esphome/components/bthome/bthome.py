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
    size: int
    name: str
    unit: str = const.UNIT_EMPTY
    device_class: str | None = None
    state_class: str = const.STATE_CLASS_MEASUREMENT
    kind: OTKind = OTKind.SENSOR


_TI = const.STATE_CLASS_TOTAL_INCREASING
_MA = const.STATE_CLASS_MEASUREMENT_ANGLE

OBJECT_TYPES_BY_ID: list[OT | None] = [
    OT(0x00, 1, "PACKET_ID"),
    OT(0x01, 1, "BATTERY_PCT", const.UNIT_PERCENT, const.DEVICE_CLASS_BATTERY),
    OT(0x02, 2, "TEMPERATURE_C_E2", const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    OT(0x03, 2, "HUMIDITY_PCT_E2", const.UNIT_PERCENT, const.DEVICE_CLASS_HUMIDITY),
    OT(
        0x04,
        3,
        "PRESSURE_HPA_E2",
        const.UNIT_HECTOPASCAL,
        const.DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    ),
    OT(0x05, 3, "ILLUMINANCE_LX_E2", const.UNIT_LUX, const.DEVICE_CLASS_ILLUMINANCE),
    OT(0x06, 2, "MASS_KG_E2", const.UNIT_KILOGRAM, const.DEVICE_CLASS_WEIGHT),
    OT(0x07, 2, "MASS_LB_E2", const.UNIT_POUND, const.DEVICE_CLASS_WEIGHT),
    OT(0x08, 2, "DEWPOINT_C_E2", const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    OT(0x09, 1, "COUNT_U8"),
    OT(
        0x0A,
        3,
        "ENERGY_KWH_E3",
        const.UNIT_KILOWATT_HOURS,
        const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    OT(0x0B, 3, "POWER_W_E2", const.UNIT_WATT, const.DEVICE_CLASS_POWER),
    OT(0x0C, 2, "VOLTAGE_V_E3", const.UNIT_VOLT, const.DEVICE_CLASS_VOLTAGE),
    OT(
        0x0D,
        2,
        "PM25_UGM3",
        const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        const.DEVICE_CLASS_PM25,
    ),
    OT(
        0x0E,
        2,
        "PM10_UGM3",
        const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        const.DEVICE_CLASS_PM10,
    ),
    OT(0x0F, 1, "GENERIC_BOOLEAN", kind=OTKind.BINARY_SENSOR),
    OT(0x10, 1, "POWER_ON", kind=OTKind.BINARY_SENSOR),
    OT(
        0x11,
        1,
        "OPENING_OPEN",
        device_class=const.DEVICE_CLASS_OPENING,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x12,
        2,
        "CO2_PPM",
        const.UNIT_PARTS_PER_MILLION,
        const.DEVICE_CLASS_CARBON_DIOXIDE,
    ),
    OT(
        0x13,
        2,
        "TVOC_UGM3",
        const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        const.DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ),
    OT(0x14, 2, "MOISTURE_PCT_E2", const.UNIT_PERCENT, const.DEVICE_CLASS_MOISTURE),
    OT(
        0x15,
        1,
        "BATTERY_LOW",
        device_class=const.DEVICE_CLASS_BATTERY,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x16,
        1,
        "BATTERY_CHARGING",
        device_class=const.DEVICE_CLASS_BATTERY_CHARGING,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x17,
        1,
        "CO_DETECTED",
        device_class=const.DEVICE_CLASS_CARBON_MONOXIDE,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x18,
        1,
        "COLD_DETECTED",
        device_class=const.DEVICE_CLASS_COLD,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x19,
        1,
        "CONNECTIVITY_CONNECTED",
        device_class=const.DEVICE_CLASS_CONNECTIVITY,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1A,
        1,
        "DOOR_OPEN",
        device_class=const.DEVICE_CLASS_DOOR,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1B,
        1,
        "GARAGE_DOOR_OPEN",
        device_class=const.DEVICE_CLASS_OPENING,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1C,
        1,
        "GAS_DETECTED",
        device_class=const.DEVICE_CLASS_GAS,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1D,
        1,
        "HEAT_DETECTED",
        device_class=const.DEVICE_CLASS_HEAT,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1E,
        1,
        "LIGHT_DETECTED",
        device_class=const.DEVICE_CLASS_LIGHT,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x1F,
        1,
        "LOCK_UNLOCKED",
        device_class=const.DEVICE_CLASS_LOCK,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x20,
        1,
        "MOISTURE_WET",
        device_class=const.DEVICE_CLASS_MOISTURE,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x21,
        1,
        "MOTION_DETECTED",
        device_class=const.DEVICE_CLASS_MOTION,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x22,
        1,
        "MOVING_ACTIVE",
        device_class=const.DEVICE_CLASS_MOVING,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x23,
        1,
        "OCCUPANCY_DETECTED",
        device_class=const.DEVICE_CLASS_OCCUPANCY,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x24,
        1,
        "PLUG_PLUGGED_IN",
        device_class=const.DEVICE_CLASS_PLUG,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x25,
        1,
        "PRESENCE_HOME",
        device_class=const.DEVICE_CLASS_PRESENCE,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x26,
        1,
        "PROBLEM_DETECTED",
        device_class=const.DEVICE_CLASS_PROBLEM,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x27,
        1,
        "RUNNING_ACTIVE",
        device_class=const.DEVICE_CLASS_RUNNING,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x28,
        1,
        "SAFETY_SAFE",
        device_class=const.DEVICE_CLASS_SAFETY,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x29,
        1,
        "SMOKE_DETECTED",
        device_class=const.DEVICE_CLASS_SMOKE,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x2A,
        1,
        "SOUND_DETECTED",
        device_class=const.DEVICE_CLASS_SOUND,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x2B,
        1,
        "TAMPER_ACTIVE",
        device_class=const.DEVICE_CLASS_TAMPER,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x2C,
        1,
        "VIBRATION_DETECTED",
        device_class=const.DEVICE_CLASS_VIBRATION,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(
        0x2D,
        1,
        "WINDOW_OPEN",
        device_class=const.DEVICE_CLASS_WINDOW,
        kind=OTKind.BINARY_SENSOR,
    ),
    OT(0x2E, 1, "HUMIDITY_PCT_U8", const.UNIT_PERCENT, const.DEVICE_CLASS_HUMIDITY),
    OT(0x2F, 1, "MOISTURE_PCT_U8", const.UNIT_PERCENT, const.DEVICE_CLASS_MOISTURE),
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    None,
    OT(0x3D, 2, "COUNT_U16"),
    OT(0x3E, 4, "COUNT_U32"),
    OT(0x3F, 2, "ROTATION_DEG_E1", const.UNIT_DEGREES, state_class=_MA),
    OT(0x40, 2, "DISTANCE_MM", const.UNIT_MILLIMETER, const.DEVICE_CLASS_DISTANCE),
    OT(0x41, 2, "DISTANCE_M_E1", const.UNIT_METER, const.DEVICE_CLASS_DISTANCE),
    OT(0x42, 3, "DURATION_S_E3", const.UNIT_SECOND, const.DEVICE_CLASS_DURATION),
    OT(0x43, 2, "CURRENT_A_E3", const.UNIT_AMPERE, const.DEVICE_CLASS_CURRENT),
    OT(0x44, 2, "SPEED_MS_E2", const.UNIT_METER_PER_SECOND, const.DEVICE_CLASS_SPEED),
    OT(0x45, 2, "TEMPERATURE_C_E1", const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    OT(0x46, 1, "UV_INDEX_E1"),
    OT(0x47, 2, "VOLUME_L_E1", const.UNIT_LITRE, const.DEVICE_CLASS_VOLUME),
    OT(0x48, 2, "VOLUME_ML", const.UNIT_MILLILITRE, const.DEVICE_CLASS_VOLUME),
    OT(
        0x49,
        2,
        "VOLUME_FLOW_M3HR_E3",
        const.UNIT_CUBIC_METER_PER_HOUR,
        const.DEVICE_CLASS_VOLUME_FLOW_RATE,
    ),
    OT(0x4A, 2, "VOLTAGE_V_E1", const.UNIT_VOLT, const.DEVICE_CLASS_VOLTAGE),
    OT(
        0x4B,
        3,
        "GAS_M3_U24_E3",
        const.UNIT_CUBIC_METER,
        const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    OT(
        0x4C,
        4,
        "GAS_M3_U32_E3",
        const.UNIT_CUBIC_METER,
        const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    OT(
        0x4D,
        4,
        "ENERGY_KWH_U32_E3",
        const.UNIT_KILOWATT_HOURS,
        const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    OT(0x4E, 4, "VOLUME_L_U32_E3", const.UNIT_LITRE, const.DEVICE_CLASS_VOLUME),
    OT(
        0x4F,
        4,
        "WATER_L_E3",
        const.UNIT_LITRE,
        const.DEVICE_CLASS_WATER,
        state_class=_TI,
    ),
    OT(0x50, 4, "TIMESTAMP", device_class=const.DEVICE_CLASS_TIMESTAMP),
    OT(0x51, 2, "ACCELERATION_MSS_E3", const.UNIT_METER_PER_SECOND_SQUARED),
    OT(0x52, 2, "GYROSCOPE_DEGS_E3", const.UNIT_DEGREE_PER_SECOND),
    OT(0x53, 0, "TEXT", kind=OTKind.TEXT_SENSOR),
    OT(0x54, 0, "RAW", kind=OTKind.TEXT_SENSOR),
    OT(
        0x55,
        4,
        "VOLUME_STORAGE_L_E3",
        const.UNIT_LITRE,
        const.DEVICE_CLASS_VOLUME_STORAGE,
    ),
    OT(
        0x56,
        2,
        "CONDUCTIVITY_USCM",
        const.UNIT_MICROSIEMENS_PER_CENTIMETER,
        const.DEVICE_CLASS_CONDUCTIVITY,
    ),
    OT(0x57, 1, "TEMPERATURE_C_I8", const.UNIT_CELSIUS, const.DEVICE_CLASS_TEMPERATURE),
    OT(
        0x58,
        1,
        "TEMPERATURE_C_I8_0_35",
        const.UNIT_CELSIUS,
        const.DEVICE_CLASS_TEMPERATURE,
    ),
    OT(0x59, 1, "COUNT_I8"),
    OT(0x5A, 2, "COUNT_I16"),
    OT(0x5B, 4, "COUNT_I32"),
    OT(0x5C, 4, "POWER_W_I32_E2", const.UNIT_WATT, const.DEVICE_CLASS_POWER),
    OT(0x5D, 2, "CURRENT_A_I16_E3", const.UNIT_AMPERE, const.DEVICE_CLASS_CURRENT),
    OT(
        0x5E,
        2,
        "DIRECTION_DEG_E2",
        const.UNIT_DEGREES,
        const.DEVICE_CLASS_WIND_DIRECTION,
        state_class=_MA,
    ),
    OT(
        0x5F,
        2,
        "PRECIPITATION_MM_E1",
        const.UNIT_MILLIMETER,
        const.DEVICE_CLASS_PRECIPITATION,
    ),
    OT(0x60, 1, "CHANNEL"),
    OT(0x61, 2, "ROTATIONAL_SPEED_RPM", const.UNIT_REVOLUTIONS_PER_MINUTE),
    OT(
        0x62,
        4,
        "SPEED_MS_I32_E6",
        const.UNIT_METER_PER_SECOND,
        const.DEVICE_CLASS_SPEED,
    ),
    OT(0x63, 4, "ACCELERATION_MSS_I32_E6", const.UNIT_METER_PER_SECOND_SQUARED),
]


BTHOME_OBJECT_TYPES: dict[str, OT] = {
    ot.name: ot for ot in OBJECT_TYPES_BY_ID if ot is not None
}
