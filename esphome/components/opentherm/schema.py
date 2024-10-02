# This file contains a schema for all supported sensors, binary sensors and
# inputs of the OpenTherm component.

from dataclasses import dataclass
from typing import Optional, TypeVar

from esphome.const import (
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_KILOWATT,
    UNIT_MICROAMP,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    STATE_CLASS_TOTAL_INCREASING,
)


@dataclass
class EntitySchema:
    description: str
    """Description of the item, based on the OpenTherm spec"""

    message: str
    """OpenTherm message id used to read or write the value"""

    keep_updated: bool
    """Whether the value should be read or write repeatedly (True) or only during
    the initialization phase (False)
    """

    message_data: str
    """Instructions on how to interpret the data in the message
      - flag8_[hb|lb]_[0-7]: data is a byte of single bit flags,
                             this flag is set in the high (hb) or low byte (lb),
                             at position 0 to 7
      - u8_[hb|lb]: data is an unsigned 8-bit integer,
                    in the high (hb) or low byte (lb)
      - s8_[hb|lb]: data is an signed 8-bit integer,
                    in the high (hb) or low byte (lb)
      - f88: data is a signed fixed point value with
              1 sign bit, 7 integer bits, 8 fractional bits
      - u16: data is an unsigned 16-bit integer
      - s16: data is a signed 16-bit integer
    """


TSchema = TypeVar("TSchema", bound=EntitySchema)


@dataclass
class SensorSchema(EntitySchema):
    accuracy_decimals: int
    state_class: str
    unit_of_measurement: Optional[str] = None
    icon: Optional[str] = None
    device_class: Optional[str] = None
    disabled_by_default: bool = False


SENSORS: dict[str, SensorSchema] = {
    "rel_mod_level": SensorSchema(
        description="Relative modulation level",
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        icon="mdi:percent",
        state_class=STATE_CLASS_MEASUREMENT,
        message="MODULATION_LEVEL",
        keep_updated=True,
        message_data="f88",
    ),
    "ch_pressure": SensorSchema(
        description="Water pressure in CH circuit",
        unit_of_measurement="bar",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_PRESSURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="CH_WATER_PRESSURE",
        keep_updated=True,
        message_data="f88",
    ),
    "dhw_flow_rate": SensorSchema(
        description="Water flow rate in DHW circuit",
        unit_of_measurement="l/min",
        accuracy_decimals=2,
        icon="mdi:waves-arrow-right",
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW_FLOW_RATE",
        keep_updated=True,
        message_data="f88",
    ),
    "t_boiler": SensorSchema(
        description="Boiler water temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FEED_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_dhw": SensorSchema(
        description="DHW temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_outside": SensorSchema(
        description="Outside temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="OUTSIDE_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_ret": SensorSchema(
        description="Return water temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="RETURN_WATER_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_storage": SensorSchema(
        description="Solar storage temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="SOLAR_STORE_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_collector": SensorSchema(
        description="Solar collector temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="SOLAR_COLLECT_TEMP",
        keep_updated=True,
        message_data="s16",
    ),
    "t_flow_ch2": SensorSchema(
        description="Flow water temperature CH2 circuit",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FEED_TEMP_CH2",
        keep_updated=True,
        message_data="f88",
    ),
    "t_dhw2": SensorSchema(
        description="Domestic hot water temperature 2",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW2_TEMP",
        keep_updated=True,
        message_data="f88",
    ),
    "t_exhaust": SensorSchema(
        description="Boiler exhaust temperature",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="EXHAUST_TEMP",
        keep_updated=True,
        message_data="s16",
    ),
    "fan_speed": SensorSchema(
        description="Boiler fan speed",
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FAN_SPEED",
        keep_updated=True,
        message_data="u16",
    ),
    "flame_current": SensorSchema(
        description="Boiler flame current",
        unit_of_measurement=UNIT_MICROAMP,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FLAME_CURRENT",
        keep_updated=True,
        message_data="f88",
    ),
    "burner_starts": SensorSchema(
        description="Number of starts burner",
        accuracy_decimals=0,
        icon="mdi:gas-burner",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="BURNER_STARTS",
        keep_updated=True,
        message_data="u16",
    ),
    "ch_pump_starts": SensorSchema(
        description="Number of starts CH pump",
        accuracy_decimals=0,
        icon="mdi:pump",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="CH_PUMP_STARTS",
        keep_updated=True,
        message_data="u16",
    ),
    "dhw_pump_valve_starts": SensorSchema(
        description="Number of starts DHW pump/valve",
        accuracy_decimals=0,
        icon="mdi:water-pump",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="DHW_PUMP_STARTS",
        keep_updated=True,
        message_data="u16",
    ),
    "dhw_burner_starts": SensorSchema(
        description="Number of starts burner during DHW mode",
        accuracy_decimals=0,
        icon="mdi:gas-burner",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="DHW_BURNER_STARTS",
        keep_updated=True,
        message_data="u16",
    ),
    "burner_operation_hours": SensorSchema(
        description="Number of hours that burner is in operation",
        accuracy_decimals=0,
        icon="mdi:clock-outline",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="BURNER_HOURS",
        keep_updated=True,
        message_data="u16",
    ),
    "ch_pump_operation_hours": SensorSchema(
        description="Number of hours that CH pump has been running",
        accuracy_decimals=0,
        icon="mdi:clock-outline",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="CH_PUMP_HOURS",
        keep_updated=True,
        message_data="u16",
    ),
    "dhw_pump_valve_operation_hours": SensorSchema(
        description="Number of hours that DHW pump has been running or DHW valve has been opened",
        accuracy_decimals=0,
        icon="mdi:clock-outline",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="DHW_PUMP_HOURS",
        keep_updated=True,
        message_data="u16",
    ),
    "dhw_burner_operation_hours": SensorSchema(
        description="Number of hours that burner is in operation during DHW mode",
        accuracy_decimals=0,
        icon="mdi:clock-outline",
        state_class=STATE_CLASS_TOTAL_INCREASING,
        message="DHW_BURNER_HOURS",
        keep_updated=True,
        message_data="u16",
    ),
    "t_dhw_set_ub": SensorSchema(
        description="Upper bound for adjustment of DHW setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW_BOUNDS",
        keep_updated=False,
        message_data="s8_hb",
    ),
    "t_dhw_set_lb": SensorSchema(
        description="Lower bound for adjustment of DHW setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW_BOUNDS",
        keep_updated=False,
        message_data="s8_lb",
    ),
    "max_t_set_ub": SensorSchema(
        description="Upper bound for adjustment of max CH setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="CH_BOUNDS",
        keep_updated=False,
        message_data="s8_hb",
    ),
    "max_t_set_lb": SensorSchema(
        description="Lower bound for adjustment of max CH setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="CH_BOUNDS",
        keep_updated=False,
        message_data="s8_lb",
    ),
    "t_dhw_set": SensorSchema(
        description="Domestic hot water temperature setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="DHW_SETPOINT",
        keep_updated=True,
        message_data="f88",
    ),
    "max_t_set": SensorSchema(
        description="Maximum allowable CH water setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        message="MAX_CH_SETPOINT",
        keep_updated=True,
        message_data="f88",
    ),
    "oem_fault_code": SensorSchema(
        description="OEM fault code",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="u8_lb",
    ),
    "oem_diagnostic_code": SensorSchema(
        description="OEM diagnostic code",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        message="OEM_DIAGNOSTIC",
        keep_updated=True,
        message_data="u16",
    ),
    "max_capacity": SensorSchema(
        description="Maximum boiler capacity (KW)",
        unit_of_measurement=UNIT_KILOWATT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        disabled_by_default=True,
        message="MAX_BOILER_CAPACITY",
        keep_updated=False,
        message_data="u8_hb",
    ),
    "min_mod_level": SensorSchema(
        description="Minimum modulation level",
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        icon="mdi:percent",
        disabled_by_default=True,
        state_class=STATE_CLASS_MEASUREMENT,
        message="MAX_BOILER_CAPACITY",
        keep_updated=False,
        message_data="u8_lb",
    ),
    "opentherm_version_device": SensorSchema(
        description="Version of OpenTherm implemented by device",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="OT_VERSION_DEVICE",
        keep_updated=False,
        message_data="f88",
    ),
    "device_type": SensorSchema(
        description="Device product type",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="VERSION_DEVICE",
        keep_updated=False,
        message_data="u8_hb",
    ),
    "device_version": SensorSchema(
        description="Device product version",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="VERSION_DEVICE",
        keep_updated=False,
        message_data="u8_lb",
    ),
    "device_id": SensorSchema(
        description="Device ID code",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="u8_lb",
    ),
    "otc_hc_ratio_ub": SensorSchema(
        description="OTC heat curve ratio upper bound",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="OTC_CURVE_BOUNDS",
        keep_updated=False,
        message_data="u8_hb",
    ),
    "otc_hc_ratio_lb": SensorSchema(
        description="OTC heat curve ratio lower bound",
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
        disabled_by_default=True,
        message="OTC_CURVE_BOUNDS",
        keep_updated=False,
        message_data="u8_lb",
    ),
}
