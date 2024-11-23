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
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_PROBLEM,
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
        icon="mdi:fan",
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FAN_SPEED",
        keep_updated=True,
        message_data="u8_lb_60",
    ),
    "fan_speed_setpoint": SensorSchema(
        description="Boiler fan speed setpoint",
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
        icon="mdi:fan",
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
        message="FAN_SPEED",
        keep_updated=True,
        message_data="u8_hb_60",
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


@dataclass
class BinarySensorSchema(EntitySchema):
    icon: Optional[str] = None
    device_class: Optional[str] = None


BINARY_SENSORS: dict[str, BinarySensorSchema] = {
    "fault_indication": BinarySensorSchema(
        description="Status: Fault indication",
        device_class=DEVICE_CLASS_PROBLEM,
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_0",
    ),
    "ch_active": BinarySensorSchema(
        description="Status: Central Heating active",
        device_class=DEVICE_CLASS_HEAT,
        icon="mdi:radiator",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_1",
    ),
    "dhw_active": BinarySensorSchema(
        description="Status: Domestic Hot Water active",
        device_class=DEVICE_CLASS_HEAT,
        icon="mdi:faucet",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_2",
    ),
    "flame_on": BinarySensorSchema(
        description="Status: Flame on",
        device_class=DEVICE_CLASS_HEAT,
        icon="mdi:fire",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_3",
    ),
    "cooling_active": BinarySensorSchema(
        description="Status: Cooling active",
        device_class=DEVICE_CLASS_COLD,
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_4",
    ),
    "ch2_active": BinarySensorSchema(
        description="Status: Central Heating 2 active",
        device_class=DEVICE_CLASS_HEAT,
        icon="mdi:radiator",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_5",
    ),
    "diagnostic_indication": BinarySensorSchema(
        description="Status: Diagnostic event",
        device_class=DEVICE_CLASS_PROBLEM,
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_6",
    ),
    "electricity_production": BinarySensorSchema(
        description="Status: Electricity production",
        device_class=DEVICE_CLASS_PROBLEM,
        message="STATUS",
        keep_updated=True,
        message_data="flag8_lb_7",
    ),
    "dhw_present": BinarySensorSchema(
        description="Configuration: DHW present",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_0",
    ),
    "control_type_on_off": BinarySensorSchema(
        description="Configuration: Control type is on/off",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_1",
    ),
    "cooling_supported": BinarySensorSchema(
        description="Configuration: Cooling supported",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_2",
    ),
    "dhw_storage_tank": BinarySensorSchema(
        description="Configuration: DHW storage tank",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_3",
    ),
    "controller_pump_control_allowed": BinarySensorSchema(
        description="Configuration: Controller pump control allowed",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_4",
    ),
    "ch2_present": BinarySensorSchema(
        description="Configuration: CH2 present",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_5",
    ),
    "water_filling": BinarySensorSchema(
        description="Configuration: Remote water filling",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_6",
    ),
    "heat_mode": BinarySensorSchema(
        description="Configuration: Heating or cooling",
        message="DEVICE_CONFIG",
        keep_updated=False,
        message_data="flag8_hb_7",
    ),
    "dhw_setpoint_transfer_enabled": BinarySensorSchema(
        description="Remote boiler parameters: DHW setpoint transfer enabled",
        message="REMOTE",
        keep_updated=False,
        message_data="flag8_hb_0",
    ),
    "max_ch_setpoint_transfer_enabled": BinarySensorSchema(
        description="Remote boiler parameters: CH maximum setpoint transfer enabled",
        message="REMOTE",
        keep_updated=False,
        message_data="flag8_hb_1",
    ),
    "dhw_setpoint_rw": BinarySensorSchema(
        description="Remote boiler parameters: DHW setpoint read/write",
        message="REMOTE",
        keep_updated=False,
        message_data="flag8_lb_0",
    ),
    "max_ch_setpoint_rw": BinarySensorSchema(
        description="Remote boiler parameters: CH maximum setpoint read/write",
        message="REMOTE",
        keep_updated=False,
        message_data="flag8_lb_1",
    ),
    "service_request": BinarySensorSchema(
        description="Service required",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_0",
    ),
    "lockout_reset": BinarySensorSchema(
        description="Lockout Reset",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_1",
    ),
    "low_water_pressure": BinarySensorSchema(
        description="Low water pressure fault",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_2",
    ),
    "flame_fault": BinarySensorSchema(
        description="Flame fault",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_3",
    ),
    "air_pressure_fault": BinarySensorSchema(
        description="Air pressure fault",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_4",
    ),
    "water_over_temp": BinarySensorSchema(
        description="Water overtemperature",
        device_class=DEVICE_CLASS_PROBLEM,
        message="FAULT_FLAGS",
        keep_updated=True,
        message_data="flag8_hb_5",
    ),
}


@dataclass
class SwitchSchema(EntitySchema):
    default_mode: Optional[str] = None


SWITCHES: dict[str, SwitchSchema] = {
    "ch_enable": SwitchSchema(
        description="Central Heating enabled",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_0",
        default_mode="restore_default_off",
    ),
    "dhw_enable": SwitchSchema(
        description="Domestic Hot Water enabled",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_1",
        default_mode="restore_default_off",
    ),
    "cooling_enable": SwitchSchema(
        description="Cooling enabled",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_2",
        default_mode="restore_default_off",
    ),
    "otc_active": SwitchSchema(
        description="Outside temperature compensation active",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_3",
        default_mode="restore_default_off",
    ),
    "ch2_active": SwitchSchema(
        description="Central Heating 2 active",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_4",
        default_mode="restore_default_off",
    ),
    "summer_mode_active": SwitchSchema(
        description="Summer mode active",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_5",
        default_mode="restore_default_off",
    ),
    "dhw_block": SwitchSchema(
        description="DHW blocked",
        message="STATUS",
        keep_updated=True,
        message_data="flag8_hb_6",
        default_mode="restore_default_off",
    ),
}


@dataclass
class AutoConfigure:
    message: str
    message_data: str


@dataclass
class InputSchema(EntitySchema):
    unit_of_measurement: str
    step: float
    range: tuple[int, int]
    icon: Optional[str] = None
    auto_max_value: Optional[AutoConfigure] = None
    auto_min_value: Optional[AutoConfigure] = None


INPUTS: dict[str, InputSchema] = {
    "t_set": InputSchema(
        description="Control setpoint: temperature setpoint for the boiler's supply water",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="CH_SETPOINT",
        keep_updated=True,
        message_data="f88",
        range=(0, 100),
        auto_max_value=AutoConfigure(message="MAX_CH_SETPOINT", message_data="f88"),
    ),
    "t_set_ch2": InputSchema(
        description="Control setpoint 2: temperature setpoint for the boiler's supply water on the second heating circuit",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="CH2_SETPOINT",
        keep_updated=True,
        message_data="f88",
        range=(0, 100),
        auto_max_value=AutoConfigure(message="MAX_CH_SETPOINT", message_data="f88"),
    ),
    "cooling_control": InputSchema(
        description="Cooling control signal",
        unit_of_measurement=UNIT_PERCENT,
        step=1.0,
        message="COOLING_CONTROL",
        keep_updated=True,
        message_data="f88",
        range=(0, 100),
    ),
    "t_dhw_set": InputSchema(
        description="Domestic hot water temperature setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="DHW_SETPOINT",
        keep_updated=True,
        message_data="f88",
        range=(0, 127),
        auto_min_value=AutoConfigure(message="DHW_BOUNDS", message_data="s8_lb"),
        auto_max_value=AutoConfigure(message="DHW_BOUNDS", message_data="s8_hb"),
    ),
    "max_t_set": InputSchema(
        description="Maximum allowable CH water setpoint",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="MAX_CH_SETPOINT",
        keep_updated=True,
        message_data="f88",
        range=(0, 127),
        auto_min_value=AutoConfigure(message="CH_BOUNDS", message_data="s8_lb"),
        auto_max_value=AutoConfigure(message="CH_BOUNDS", message_data="s8_hb"),
    ),
    "t_room_set": InputSchema(
        description="Current room temperature setpoint (informational)",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="ROOM_SETPOINT",
        keep_updated=True,
        message_data="f88",
        range=(-40, 127),
    ),
    "t_room_set_ch2": InputSchema(
        description="Current room temperature setpoint on CH2 (informational)",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="ROOM_SETPOINT_CH2",
        keep_updated=True,
        message_data="f88",
        range=(-40, 127),
    ),
    "t_room": InputSchema(
        description="Current sensed room temperature (informational)",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="ROOM_TEMP",
        keep_updated=True,
        message_data="f88",
        range=(-40, 127),
    ),
    "max_rel_mod_level": InputSchema(
        description="Maximum relative modulation level",
        unit_of_measurement=UNIT_PERCENT,
        step=1,
        icon="mdi:percent",
        message="MAX_MODULATION_LEVEL",
        keep_updated=True,
        message_data="f88",
        range=(0, 100),
    ),
    "otc_hc_ratio": InputSchema(
        description="OTC heat curve ratio",
        unit_of_measurement=UNIT_CELSIUS,
        step=0.1,
        message="OTC_CURVE_RATIO",
        keep_updated=True,
        message_data="f88",
        range=(0, 127),
        auto_min_value=AutoConfigure(message="OTC_CURVE_BOUNDS", message_data="u8_lb"),
        auto_max_value=AutoConfigure(message="OTC_CURVE_BOUNDS", message_data="u8_hb"),
    ),
}
