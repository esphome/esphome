from dataclasses import dataclass
from enum import Enum, auto

from esphome import const
import esphome.codegen as cg
import esphome.config_validation as cv

bthome_ns = cg.esphome_ns.namespace("bthome")
bthome_object_types = bthome_ns.enum("BTHomeObjectType", True)


BTHOME_SERVER_MAX_PAYLOAD = 23  # BLE_ADV_MAX_SIZE - BLE_FLAGS_SIZE - BLE_ADV_HEADER_SIZE - sizeof(esphome::bthome::BTHomeHeader)
BTHOME_SERVER_MAX_ENCRYPTED_PAYLOAD = (
    15  # BTHOME_SERVER_MAX_PAYLOAD - 4 (MIC) - 4 (Counter)
)

CONF_BTHOME_TYPE = "bthome_type"


class BTHomeObjectTypeKind(Enum):
    SENSOR = auto()
    BINARY_SENSOR = auto()
    TEXT_SENSOR = auto()


@dataclass
class BTHomeObjectType:
    """BTHome object type descriptor with sensor display defaults."""

    object_id: int
    size: int
    name: str
    unit: str = const.UNIT_EMPTY
    device_class: str | None = None
    state_class: str = const.STATE_CLASS_MEASUREMENT
    kind: BTHomeObjectTypeKind = BTHomeObjectTypeKind.SENSOR


_TI = const.STATE_CLASS_TOTAL_INCREASING
_MA = const.STATE_CLASS_MEASUREMENT_ANGLE

OBJECT_TYPES_BY_ID: list[BTHomeObjectType | None] = [
    BTHomeObjectType(object_id=0x00, size=1, name="PACKET_ID"),
    BTHomeObjectType(
        object_id=0x01,
        size=1,
        name="BATTERY_PCT",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_BATTERY,
    ),
    BTHomeObjectType(
        object_id=0x02,
        size=2,
        name="TEMPERATURE_C_E2",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(
        object_id=0x03,
        size=2,
        name="HUMIDITY_PCT_E2",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_HUMIDITY,
    ),
    BTHomeObjectType(
        object_id=0x04,
        size=3,
        name="PRESSURE_HPA_E2",
        unit=const.UNIT_HECTOPASCAL,
        device_class=const.DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    ),
    BTHomeObjectType(
        object_id=0x05,
        size=3,
        name="ILLUMINANCE_LX_E2",
        unit=const.UNIT_LUX,
        device_class=const.DEVICE_CLASS_ILLUMINANCE,
    ),
    BTHomeObjectType(
        object_id=0x06,
        size=2,
        name="MASS_KG_E2",
        unit=const.UNIT_KILOGRAM,
        device_class=const.DEVICE_CLASS_WEIGHT,
    ),
    BTHomeObjectType(
        object_id=0x07,
        size=2,
        name="MASS_LB_E2",
        unit=const.UNIT_POUND,
        device_class=const.DEVICE_CLASS_WEIGHT,
    ),
    BTHomeObjectType(
        object_id=0x08,
        size=2,
        name="DEWPOINT_C_E2",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x09, size=1, name="COUNT_U8"),
    BTHomeObjectType(
        object_id=0x0A,
        size=3,
        name="ENERGY_KWH_E3",
        unit=const.UNIT_KILOWATT_HOURS,
        device_class=const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x0B,
        size=3,
        name="POWER_W_E2",
        unit=const.UNIT_WATT,
        device_class=const.DEVICE_CLASS_POWER,
    ),
    BTHomeObjectType(
        object_id=0x0C,
        size=2,
        name="VOLTAGE_V_E3",
        unit=const.UNIT_VOLT,
        device_class=const.DEVICE_CLASS_VOLTAGE,
    ),
    BTHomeObjectType(
        object_id=0x0D,
        size=2,
        name="PM25_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_PM25,
    ),
    BTHomeObjectType(
        object_id=0x0E,
        size=2,
        name="PM10_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_PM10,
    ),
    BTHomeObjectType(
        object_id=0x0F,
        size=1,
        name="GENERIC_BOOLEAN",
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x10, size=1, name="POWER_ON", kind=BTHomeObjectTypeKind.BINARY_SENSOR
    ),
    BTHomeObjectType(
        object_id=0x11,
        size=1,
        name="OPENING_OPEN",
        device_class=const.DEVICE_CLASS_OPENING,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x12,
        size=2,
        name="CO2_PPM",
        unit=const.UNIT_PARTS_PER_MILLION,
        device_class=const.DEVICE_CLASS_CARBON_DIOXIDE,
    ),
    BTHomeObjectType(
        object_id=0x13,
        size=2,
        name="TVOC_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ),
    BTHomeObjectType(
        object_id=0x14,
        size=2,
        name="MOISTURE_PCT_E2",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_MOISTURE,
    ),
    BTHomeObjectType(
        object_id=0x15,
        size=1,
        name="BATTERY_LOW",
        device_class=const.DEVICE_CLASS_BATTERY,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x16,
        size=1,
        name="BATTERY_CHARGING",
        device_class=const.DEVICE_CLASS_BATTERY_CHARGING,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x17,
        size=1,
        name="CO_DETECTED",
        device_class=const.DEVICE_CLASS_CARBON_MONOXIDE,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x18,
        size=1,
        name="COLD_DETECTED",
        device_class=const.DEVICE_CLASS_COLD,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x19,
        size=1,
        name="CONNECTIVITY_CONNECTED",
        device_class=const.DEVICE_CLASS_CONNECTIVITY,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1A,
        size=1,
        name="DOOR_OPEN",
        device_class=const.DEVICE_CLASS_DOOR,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1B,
        size=1,
        name="GARAGE_DOOR_OPEN",
        device_class=const.DEVICE_CLASS_OPENING,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1C,
        size=1,
        name="GAS_DETECTED",
        device_class=const.DEVICE_CLASS_GAS,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1D,
        size=1,
        name="HEAT_DETECTED",
        device_class=const.DEVICE_CLASS_HEAT,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1E,
        size=1,
        name="LIGHT_DETECTED",
        device_class=const.DEVICE_CLASS_LIGHT,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x1F,
        size=1,
        name="LOCK_UNLOCKED",
        device_class=const.DEVICE_CLASS_LOCK,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x20,
        size=1,
        name="MOISTURE_WET",
        device_class=const.DEVICE_CLASS_MOISTURE,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x21,
        size=1,
        name="MOTION_DETECTED",
        device_class=const.DEVICE_CLASS_MOTION,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x22,
        size=1,
        name="MOVING_ACTIVE",
        device_class=const.DEVICE_CLASS_MOVING,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x23,
        size=1,
        name="OCCUPANCY_DETECTED",
        device_class=const.DEVICE_CLASS_OCCUPANCY,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x24,
        size=1,
        name="PLUG_PLUGGED_IN",
        device_class=const.DEVICE_CLASS_PLUG,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x25,
        size=1,
        name="PRESENCE_HOME",
        device_class=const.DEVICE_CLASS_PRESENCE,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x26,
        size=1,
        name="PROBLEM_DETECTED",
        device_class=const.DEVICE_CLASS_PROBLEM,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x27,
        size=1,
        name="RUNNING_ACTIVE",
        device_class=const.DEVICE_CLASS_RUNNING,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x28,
        size=1,
        name="SAFETY_SAFE",
        device_class=const.DEVICE_CLASS_SAFETY,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x29,
        size=1,
        name="SMOKE_DETECTED",
        device_class=const.DEVICE_CLASS_SMOKE,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x2A,
        size=1,
        name="SOUND_DETECTED",
        device_class=const.DEVICE_CLASS_SOUND,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x2B,
        size=1,
        name="TAMPER_ACTIVE",
        device_class=const.DEVICE_CLASS_TAMPER,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x2C,
        size=1,
        name="VIBRATION_DETECTED",
        device_class=const.DEVICE_CLASS_VIBRATION,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x2D,
        size=1,
        name="WINDOW_OPEN",
        device_class=const.DEVICE_CLASS_WINDOW,
        kind=BTHomeObjectTypeKind.BINARY_SENSOR,
    ),
    BTHomeObjectType(
        object_id=0x2E,
        size=1,
        name="HUMIDITY_PCT_U8",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_HUMIDITY,
    ),
    BTHomeObjectType(
        object_id=0x2F,
        size=1,
        name="MOISTURE_PCT_U8",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_MOISTURE,
    ),
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
    BTHomeObjectType(object_id=0x3D, size=2, name="COUNT_U16"),
    BTHomeObjectType(object_id=0x3E, size=4, name="COUNT_U32"),
    BTHomeObjectType(
        object_id=0x3F,
        size=2,
        name="ROTATION_DEG_E1",
        unit=const.UNIT_DEGREES,
        state_class=_MA,
    ),
    BTHomeObjectType(
        object_id=0x40,
        size=2,
        name="DISTANCE_MM",
        unit=const.UNIT_MILLIMETER,
        device_class=const.DEVICE_CLASS_DISTANCE,
    ),
    BTHomeObjectType(
        object_id=0x41,
        size=2,
        name="DISTANCE_M_E1",
        unit=const.UNIT_METER,
        device_class=const.DEVICE_CLASS_DISTANCE,
    ),
    BTHomeObjectType(
        object_id=0x42,
        size=3,
        name="DURATION_S_E3",
        unit=const.UNIT_SECOND,
        device_class=const.DEVICE_CLASS_DURATION,
    ),
    BTHomeObjectType(
        object_id=0x43,
        size=2,
        name="CURRENT_A_E3",
        unit=const.UNIT_AMPERE,
        device_class=const.DEVICE_CLASS_CURRENT,
    ),
    BTHomeObjectType(
        object_id=0x44,
        size=2,
        name="SPEED_MS_E2",
        unit=const.UNIT_METER_PER_SECOND,
        device_class=const.DEVICE_CLASS_SPEED,
    ),
    BTHomeObjectType(
        object_id=0x45,
        size=2,
        name="TEMPERATURE_C_E1",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x46, size=1, name="UV_INDEX_E1"),
    BTHomeObjectType(
        object_id=0x47,
        size=2,
        name="VOLUME_L_E1",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
    ),
    BTHomeObjectType(
        object_id=0x48,
        size=2,
        name="VOLUME_ML",
        unit=const.UNIT_MILLILITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
    ),
    BTHomeObjectType(
        object_id=0x49,
        size=2,
        name="VOLUME_FLOW_M3HR_E3",
        unit=const.UNIT_CUBIC_METER_PER_HOUR,
        device_class=const.DEVICE_CLASS_VOLUME_FLOW_RATE,
    ),
    BTHomeObjectType(
        object_id=0x4A,
        size=2,
        name="VOLTAGE_V_E1",
        unit=const.UNIT_VOLT,
        device_class=const.DEVICE_CLASS_VOLTAGE,
    ),
    BTHomeObjectType(
        object_id=0x4B,
        size=3,
        name="GAS_M3_U24_E3",
        unit=const.UNIT_CUBIC_METER,
        device_class=const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4C,
        size=4,
        name="GAS_M3_U32_E3",
        unit=const.UNIT_CUBIC_METER,
        device_class=const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4D,
        size=4,
        name="ENERGY_KWH_U32_E3",
        unit=const.UNIT_KILOWATT_HOURS,
        device_class=const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4E,
        size=4,
        name="VOLUME_L_U32_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
    ),
    BTHomeObjectType(
        object_id=0x4F,
        size=4,
        name="WATER_L_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_WATER,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x50,
        size=4,
        name="TIMESTAMP",
        device_class=const.DEVICE_CLASS_TIMESTAMP,
    ),
    BTHomeObjectType(
        object_id=0x51,
        size=2,
        name="ACCELERATION_MSS_E3",
        unit=const.UNIT_METER_PER_SECOND_SQUARED,
    ),
    BTHomeObjectType(
        object_id=0x52,
        size=2,
        name="GYROSCOPE_DEGS_E3",
        unit=const.UNIT_DEGREE_PER_SECOND,
    ),
    BTHomeObjectType(
        object_id=0x53, size=0, name="TEXT", kind=BTHomeObjectTypeKind.TEXT_SENSOR
    ),
    BTHomeObjectType(
        object_id=0x54, size=0, name="RAW", kind=BTHomeObjectTypeKind.TEXT_SENSOR
    ),
    BTHomeObjectType(
        object_id=0x55,
        size=4,
        name="VOLUME_STORAGE_L_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME_STORAGE,
    ),
    BTHomeObjectType(
        object_id=0x56,
        size=2,
        name="CONDUCTIVITY_USCM",
        unit=const.UNIT_MICROSIEMENS_PER_CENTIMETER,
        device_class=const.DEVICE_CLASS_CONDUCTIVITY,
    ),
    BTHomeObjectType(
        object_id=0x57,
        size=1,
        name="TEMPERATURE_C_I8",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(
        object_id=0x58,
        size=1,
        name="TEMPERATURE_C_I8_0_35",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x59, size=1, name="COUNT_I8"),
    BTHomeObjectType(object_id=0x5A, size=2, name="COUNT_I16"),
    BTHomeObjectType(object_id=0x5B, size=4, name="COUNT_I32"),
    BTHomeObjectType(
        object_id=0x5C,
        size=4,
        name="POWER_W_I32_E2",
        unit=const.UNIT_WATT,
        device_class=const.DEVICE_CLASS_POWER,
    ),
    BTHomeObjectType(
        object_id=0x5D,
        size=2,
        name="CURRENT_A_I16_E3",
        unit=const.UNIT_AMPERE,
        device_class=const.DEVICE_CLASS_CURRENT,
    ),
    BTHomeObjectType(
        object_id=0x5E,
        size=2,
        name="DIRECTION_DEG_E2",
        unit=const.UNIT_DEGREES,
        device_class=const.DEVICE_CLASS_WIND_DIRECTION,
        state_class=_MA,
    ),
    BTHomeObjectType(
        object_id=0x5F,
        size=2,
        name="PRECIPITATION_MM_E1",
        unit=const.UNIT_MILLIMETER,
        device_class=const.DEVICE_CLASS_PRECIPITATION,
    ),
    BTHomeObjectType(object_id=0x60, size=1, name="CHANNEL"),
    BTHomeObjectType(
        object_id=0x61,
        size=2,
        name="ROTATIONAL_SPEED_RPM",
        unit=const.UNIT_REVOLUTIONS_PER_MINUTE,
    ),
    BTHomeObjectType(
        object_id=0x62,
        size=4,
        name="SPEED_MS_I32_E6",
        unit=const.UNIT_METER_PER_SECOND,
        device_class=const.DEVICE_CLASS_SPEED,
    ),
    BTHomeObjectType(
        object_id=0x63,
        size=4,
        name="ACCELERATION_MSS_I32_E6",
        unit=const.UNIT_METER_PER_SECOND_SQUARED,
    ),
]

BTHOME_OBJECT_TYPES: dict[str, BTHomeObjectType] = {}
for index, ot in enumerate(OBJECT_TYPES_BY_ID):
    if ot is None:
        continue
    assert index == ot.object_id, (
        f"Object ID mismatch at index {index}: {ot.name} has object_id={ot.object_id}"
    )
    BTHOME_OBJECT_TYPES[ot.name] = ot

# Maps short, easy-to-remember names to the canonical object type name with the
# smallest object_id in each family.  Only sensor types are aliased; binary
# sensor names are already self-descriptive (BATTERY_LOW, POWER_ON, …) and have
# no multi-variant siblings that would benefit from disambiguation.
BTHOME_OBJECT_ALIASES: dict[str, str] = {
    "ACCELERATION": "ACCELERATION_MSS_E3",  # 0x51 (alt: MSS_I32_E6)
    "BATTERY": "BATTERY_PCT",  # 0x01
    "CO2": "CO2_PPM",  # 0x12
    "CONDUCTIVITY": "CONDUCTIVITY_USCM",  # 0x56
    "CURRENT": "CURRENT_A_E3",  # 0x43 (alt: A_I16_E3)
    "DEWPOINT": "DEWPOINT_C_E2",  # 0x08
    "DIRECTION": "DIRECTION_DEG_E2",  # 0x5E
    "DISTANCE": "DISTANCE_MM",  # 0x40 (alt: M_E1)
    "DURATION": "DURATION_S_E3",  # 0x42
    "ENERGY": "ENERGY_KWH_E3",  # 0x0A (alt: KWH_U32_E3)
    "GYROSCOPE": "GYROSCOPE_DEGS_E3",  # 0x52
    "HUMIDITY": "HUMIDITY_PCT_E2",  # 0x03 (alt: PCT_U8)
    "ILLUMINANCE": "ILLUMINANCE_LX_E2",  # 0x05
    "MASS_KG": "MASS_KG_E2",  # 0x06
    "MASS_LB": "MASS_LB_E2",  # 0x07
    "MOISTURE": "MOISTURE_PCT_E2",  # 0x14 (alt: PCT_U8)
    "PM10": "PM10_UGM3",  # 0x0E
    "PM25": "PM25_UGM3",  # 0x0D
    "POWER": "POWER_W_E2",  # 0x0B (alt: W_I32_E2)
    "PRECIPITATION": "PRECIPITATION_MM_E1",  # 0x5F
    "PRESSURE": "PRESSURE_HPA_E2",  # 0x04
    "ROTATION": "ROTATION_DEG_E1",  # 0x3F
    "ROTATIONAL_SPEED": "ROTATIONAL_SPEED_RPM",  # 0x61
    "SPEED": "SPEED_MS_E2",  # 0x44 (alt: MS_I32_E6)
    "TEMPERATURE": "TEMPERATURE_C_E2",  # 0x02 (alt: C_E1, C_I8, C_I8_0_35)
    "TVOC": "TVOC_UGM3",  # 0x13
    "UV_INDEX": "UV_INDEX_E1",  # 0x46
    "VOLTAGE": "VOLTAGE_V_E3",  # 0x0C (alt: V_E1)
    "VOLUME": "VOLUME_L_E1",  # 0x47 (alt: ML, L_U32_E3)
    "VOLUME_FLOW": "VOLUME_FLOW_M3HR_E3",  # 0x49
    "VOLUME_STORAGE": "VOLUME_STORAGE_L_E3",  # 0x55
    "WATER": "WATER_L_E3",  # 0x4F
}


def bthome_object_type_validator(kind: BTHomeObjectTypeKind):
    """Return a validator for BTHome object types of the specified kind."""

    def validator(key_or_alias: str) -> str:
        key = BTHOME_OBJECT_ALIASES.get(key_or_alias.upper(), key_or_alias)
        value = BTHOME_OBJECT_TYPES.get(key.upper())
        if value is None:
            raise cv.Invalid(f"Unknown BTHome object type: {key}")
        if value.kind != kind:
            raise cv.Invalid(f"Object type {key} is not a {kind.name}")
        return key.upper()

    return validator
