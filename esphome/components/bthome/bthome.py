from collections.abc import Callable
from dataclasses import dataclass

from esphome import const
import esphome.codegen as cg
import esphome.config_validation as cv

bthome_ns = cg.esphome_ns.namespace("bthome")
bthome_object_types = bthome_ns.enum("BTHomeObjectType", True)


@dataclass
class BTHomeObjectType:
    """BTHome object type descriptor with entity defaults.

    Value length, scale and sign live in the C++ object table in bthome.h.
    """

    object_id: int
    name: str
    unit: str = const.UNIT_EMPTY
    accuracy_decimals: int = 2
    device_class: str | None = None
    state_class: str = const.STATE_CLASS_MEASUREMENT


def object_types_by_name(*types: BTHomeObjectType) -> dict[str, BTHomeObjectType]:
    """Index object types by name, rejecting duplicate names or IDs."""
    by_name = {t.name: t for t in types}
    assert len(by_name) == len(types), "Duplicate BTHome object type name"
    assert len({t.object_id for t in types}) == len(types), "Duplicate BTHome object ID"
    return by_name


def bthome_object_type_validator(
    object_types: dict[str, BTHomeObjectType], aliases: dict[str, str] | None = None
) -> Callable[[str], str]:
    """Return a validator accepting the given object types (by name or alias)."""

    def validator(value: str) -> str:
        key = cv.string(value).upper()
        key = (aliases or {}).get(key, key)
        if key not in object_types:
            raise cv.Invalid(
                f"Unknown BTHome object type for this platform: {value}. "
                f"Valid types: {', '.join(sorted(object_types))}"
            )
        return key

    return validator


_TI = const.STATE_CLASS_TOTAL_INCREASING
_MA = const.STATE_CLASS_MEASUREMENT_ANGLE

SENSOR_OBJECT_TYPES = object_types_by_name(
    BTHomeObjectType(object_id=0x00, name="PACKET_ID"),
    BTHomeObjectType(
        object_id=0x01,
        name="BATTERY_PCT",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_BATTERY,
    ),
    BTHomeObjectType(
        object_id=0x02,
        name="TEMPERATURE_C_E2",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(
        object_id=0x03,
        name="HUMIDITY_PCT_E2",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_HUMIDITY,
    ),
    BTHomeObjectType(
        object_id=0x04,
        name="PRESSURE_HPA_E2",
        unit=const.UNIT_HECTOPASCAL,
        device_class=const.DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    ),
    BTHomeObjectType(
        object_id=0x05,
        name="ILLUMINANCE_LX_E2",
        unit=const.UNIT_LUX,
        device_class=const.DEVICE_CLASS_ILLUMINANCE,
    ),
    BTHomeObjectType(
        object_id=0x06,
        name="MASS_KG_E2",
        unit=const.UNIT_KILOGRAM,
        device_class=const.DEVICE_CLASS_WEIGHT,
    ),
    BTHomeObjectType(
        object_id=0x07,
        name="MASS_LB_E2",
        unit=const.UNIT_POUND,
        device_class=const.DEVICE_CLASS_WEIGHT,
    ),
    BTHomeObjectType(
        object_id=0x08,
        name="DEWPOINT_C_E2",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x09, name="COUNT_U8"),
    BTHomeObjectType(
        object_id=0x0A,
        name="ENERGY_KWH_E3",
        unit=const.UNIT_KILOWATT_HOURS,
        device_class=const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x0B,
        name="POWER_W_E2",
        unit=const.UNIT_WATT,
        device_class=const.DEVICE_CLASS_POWER,
    ),
    BTHomeObjectType(
        object_id=0x0C,
        name="VOLTAGE_V_E3",
        unit=const.UNIT_VOLT,
        device_class=const.DEVICE_CLASS_VOLTAGE,
    ),
    BTHomeObjectType(
        object_id=0x0D,
        name="PM2_5_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_PM25,
    ),
    BTHomeObjectType(
        object_id=0x0E,
        name="PM10_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_PM10,
    ),
    BTHomeObjectType(
        object_id=0x12,
        name="CO2_PPM",
        unit=const.UNIT_PARTS_PER_MILLION,
        device_class=const.DEVICE_CLASS_CARBON_DIOXIDE,
    ),
    BTHomeObjectType(
        object_id=0x13,
        name="TVOC_UGM3",
        unit=const.UNIT_MICROGRAMS_PER_CUBIC_METER,
        device_class=const.DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ),
    BTHomeObjectType(
        object_id=0x14,
        name="MOISTURE_PCT_E2",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_MOISTURE,
    ),
    BTHomeObjectType(
        object_id=0x2E,
        name="HUMIDITY_PCT_U8",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_HUMIDITY,
    ),
    BTHomeObjectType(
        object_id=0x2F,
        name="MOISTURE_PCT_U8",
        unit=const.UNIT_PERCENT,
        device_class=const.DEVICE_CLASS_MOISTURE,
    ),
    BTHomeObjectType(object_id=0x3D, name="COUNT_U16"),
    BTHomeObjectType(object_id=0x3E, name="COUNT_U32"),
    BTHomeObjectType(
        object_id=0x3F, name="ROTATION_DEG_E1", unit=const.UNIT_DEGREES, state_class=_MA
    ),
    BTHomeObjectType(
        object_id=0x40,
        name="DISTANCE_MM",
        unit=const.UNIT_MILLIMETER,
        device_class=const.DEVICE_CLASS_DISTANCE,
    ),
    BTHomeObjectType(
        object_id=0x41,
        name="DISTANCE_M_E1",
        unit=const.UNIT_METER,
        device_class=const.DEVICE_CLASS_DISTANCE,
    ),
    BTHomeObjectType(
        object_id=0x42,
        name="DURATION_S_E3",
        unit=const.UNIT_SECOND,
        device_class=const.DEVICE_CLASS_DURATION,
    ),
    BTHomeObjectType(
        object_id=0x43,
        name="CURRENT_A_E3",
        unit=const.UNIT_AMPERE,
        device_class=const.DEVICE_CLASS_CURRENT,
    ),
    BTHomeObjectType(
        object_id=0x44,
        name="SPEED_MS_E2",
        unit=const.UNIT_METER_PER_SECOND,
        device_class=const.DEVICE_CLASS_SPEED,
    ),
    BTHomeObjectType(
        object_id=0x45,
        name="TEMPERATURE_C_E1",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x46, name="UV_INDEX_E1"),
    BTHomeObjectType(
        object_id=0x47,
        name="VOLUME_L_E1",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
        state_class=const.STATE_CLASS_TOTAL,
    ),
    BTHomeObjectType(
        object_id=0x48,
        name="VOLUME_ML",
        unit=const.UNIT_MILLILITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
        state_class=const.STATE_CLASS_TOTAL,
    ),
    BTHomeObjectType(
        object_id=0x49,
        name="VOLUME_FLOW_M3HR_E3",
        unit=const.UNIT_CUBIC_METER_PER_HOUR,
        device_class=const.DEVICE_CLASS_VOLUME_FLOW_RATE,
    ),
    BTHomeObjectType(
        object_id=0x4A,
        name="VOLTAGE_V_E1",
        unit=const.UNIT_VOLT,
        device_class=const.DEVICE_CLASS_VOLTAGE,
    ),
    BTHomeObjectType(
        object_id=0x4B,
        name="GAS_M3_U24_E3",
        unit=const.UNIT_CUBIC_METER,
        device_class=const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4C,
        name="GAS_M3_U32_E3",
        unit=const.UNIT_CUBIC_METER,
        device_class=const.DEVICE_CLASS_GAS,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4D,
        name="ENERGY_KWH_U32_E3",
        unit=const.UNIT_KILOWATT_HOURS,
        device_class=const.DEVICE_CLASS_ENERGY,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x4E,
        name="VOLUME_L_U32_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME,
        state_class=const.STATE_CLASS_TOTAL,
    ),
    BTHomeObjectType(
        object_id=0x4F,
        name="WATER_L_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_WATER,
        state_class=_TI,
    ),
    BTHomeObjectType(
        object_id=0x50,
        name="TIMESTAMP",
        accuracy_decimals=0,
        device_class=const.DEVICE_CLASS_TIMESTAMP,
        state_class=const.STATE_CLASS_NONE,
    ),
    BTHomeObjectType(
        object_id=0x51,
        name="ACCELERATION_MSS_E3",
        unit=const.UNIT_METER_PER_SECOND_SQUARED,
    ),
    BTHomeObjectType(
        object_id=0x52, name="GYROSCOPE_DEGS_E3", unit=const.UNIT_DEGREE_PER_SECOND
    ),
    BTHomeObjectType(
        object_id=0x55,
        name="VOLUME_STORAGE_L_E3",
        unit=const.UNIT_LITRE,
        device_class=const.DEVICE_CLASS_VOLUME_STORAGE,
    ),
    BTHomeObjectType(
        object_id=0x56,
        name="CONDUCTIVITY_USCM",
        unit=const.UNIT_MICROSIEMENS_PER_CENTIMETER,
        device_class=const.DEVICE_CLASS_CONDUCTIVITY,
    ),
    BTHomeObjectType(
        object_id=0x57,
        name="TEMPERATURE_C_I8",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(
        object_id=0x58,
        name="TEMPERATURE_C_I8_0_35",
        unit=const.UNIT_CELSIUS,
        device_class=const.DEVICE_CLASS_TEMPERATURE,
    ),
    BTHomeObjectType(object_id=0x59, name="COUNT_I8"),
    BTHomeObjectType(object_id=0x5A, name="COUNT_I16"),
    BTHomeObjectType(object_id=0x5B, name="COUNT_I32"),
    BTHomeObjectType(
        object_id=0x5C,
        name="POWER_W_I32_E2",
        unit=const.UNIT_WATT,
        device_class=const.DEVICE_CLASS_POWER,
    ),
    BTHomeObjectType(
        object_id=0x5D,
        name="CURRENT_A_I16_E3",
        unit=const.UNIT_AMPERE,
        device_class=const.DEVICE_CLASS_CURRENT,
    ),
    BTHomeObjectType(
        object_id=0x5E,
        name="DIRECTION_DEG_E2",
        unit=const.UNIT_DEGREES,
        device_class=const.DEVICE_CLASS_WIND_DIRECTION,
        state_class=_MA,
    ),
    BTHomeObjectType(
        object_id=0x5F,
        name="PRECIPITATION_MM_E1",
        unit=const.UNIT_MILLIMETER,
        device_class=const.DEVICE_CLASS_PRECIPITATION,
    ),
    BTHomeObjectType(object_id=0x60, name="CHANNEL"),
    BTHomeObjectType(
        object_id=0x61,
        name="ROTATIONAL_SPEED_RPM",
        unit=const.UNIT_REVOLUTIONS_PER_MINUTE,
    ),
    BTHomeObjectType(
        object_id=0x62,
        name="SPEED_MS_I32_E6",
        unit=const.UNIT_METER_PER_SECOND,
        device_class=const.DEVICE_CLASS_SPEED,
    ),
    BTHomeObjectType(
        object_id=0x63,
        name="ACCELERATION_MSS_I32_E6",
        unit=const.UNIT_METER_PER_SECOND_SQUARED,
    ),
    BTHomeObjectType(
        object_id=0x0100,
        name="SIGNAL_STRENGTH",
        unit=const.UNIT_DECIBEL_MILLIWATT,
        accuracy_decimals=0,
        device_class=const.DEVICE_CLASS_SIGNAL_STRENGTH,
    ),
)

# Maps short, easy-to-remember names to the canonical object type name.
SENSOR_OBJECT_ALIASES: dict[str, str] = {
    "ACCELERATION": "ACCELERATION_MSS_E3",
    "BATTERY": "BATTERY_PCT",
    "CO2": "CO2_PPM",
    "CONDUCTIVITY": "CONDUCTIVITY_USCM",
    "CURRENT": "CURRENT_A_E3",
    "DEWPOINT": "DEWPOINT_C_E2",
    "DIRECTION": "DIRECTION_DEG_E2",
    "DISTANCE": "DISTANCE_MM",
    "DURATION": "DURATION_S_E3",
    "ENERGY": "ENERGY_KWH_E3",
    "GYROSCOPE": "GYROSCOPE_DEGS_E3",
    "HUMIDITY": "HUMIDITY_PCT_E2",
    "ILLUMINANCE": "ILLUMINANCE_LX_E2",
    "MASS_KG": "MASS_KG_E2",
    "MASS_LB": "MASS_LB_E2",
    "MOISTURE": "MOISTURE_PCT_E2",
    "PM10": "PM10_UGM3",
    "PM2_5": "PM2_5_UGM3",
    "POWER": "POWER_W_E2",
    "PRECIPITATION": "PRECIPITATION_MM_E1",
    "PRESSURE": "PRESSURE_HPA_E2",
    "ROTATION": "ROTATION_DEG_E1",
    "ROTATIONAL_SPEED": "ROTATIONAL_SPEED_RPM",
    "SPEED": "SPEED_MS_E2",
    "TEMPERATURE": "TEMPERATURE_C_E2",
    "TVOC": "TVOC_UGM3",
    "UV_INDEX": "UV_INDEX_E1",
    "VOLTAGE": "VOLTAGE_V_E3",
    "VOLUME": "VOLUME_L_E1",
    "VOLUME_FLOW": "VOLUME_FLOW_M3HR_E3",
    "VOLUME_STORAGE": "VOLUME_STORAGE_L_E3",
    "WATER": "WATER_L_E3",
}
