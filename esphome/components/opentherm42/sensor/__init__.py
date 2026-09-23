import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_INDEX,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    UNIT_REVOLUTIONS_PER_MINUTE,
)

from .. import (
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_INTERVAL,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAX_CAPACITY_MIN_MOD_LEVEL_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_INTERVAL,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_INTERVAL,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_INTERVAL,
    OpenTherm42Hub,
    validate_requires_hub_option,
)
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE,
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_BOILER_CAPACITY,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MINIMUM_MODULATION_LEVEL,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_2,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_SOLAR_STORAGE,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_SOLAR_STORAGE,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY,
    CONF_OPENTHERM42_ID,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND,
    CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_EXHAUST_FAN_SPEED,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_INLET_FAN_SPEED,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_SETPOINT,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_HEAT_EXCHANGER_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_WATER_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_WATER_PRESSURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW2_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_FLOW_RATE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCTION,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_INLET_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_OUTLET_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLAME_CURRENT,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLOW_TEMPERATURE_CH2,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_SOLAR_STORAGE,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_VENTILATION_HEAT_RECOVERY,
)


# All of these sensors are boiler-reported values: on a failed conversation, every configured sensor
# here must show unknown rather than keep a stale reading.
#
# entity_category follows the data's own nature, independent of which spec class it lives in:
# - Live, changeable status/fault/mode readouts and setpoints the boiler is currently operating on
#   are primary (entity_category left unset).
# - Static identification/capability values the boiler reports once (member IDs, product
#   type/version, protocol version, TSP/fault-history-buffer counts, installation-time
#   bounds/limits) are DIAGNOSTIC -- Home Assistant's own example of a diagnostic entity is "a
#   sensor showing... MAC address", exactly this kind of read-only identifier.
# - Lifetime wear/reliability counters for an internal moving part (burner ignitions, pump starts,
#   unsuccessful starts, flame-signal faults, power cycles) are DIAGNOSTIC -- used for service
#   scheduling and troubleshooting, not day-to-day monitoring, unlike genuine usage/production
#   statistics (electricity produced, cooling hours) which stay primary.
#
# No sensor here gets a state_class of "measurement" for a discrete code (fault code, mode, product
# type/version): averaging or graphing a category number is meaningless, so codes get no state_class
# at all, the same as Home Assistant's own convention for identifier-like sensors.
def _code_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(accuracy_decimals=0, entity_category=entity_category)


# §5.1: OpenTherm protocol versions are f8.8 (e.g. 2.2, 4.2) -- two decimals is enough to show them exactly.
def _version_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(accuracy_decimals=2, entity_category=entity_category)


def _temperature_schema(
    *, accuracy_decimals: int = 2, entity_category: str = cv.UNDEFINED
) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="°C",
        accuracy_decimals=accuracy_decimals,
        device_class="temperature",
        state_class="measurement",
        entity_category=entity_category,
    )


def _percent_schema(
    *, accuracy_decimals: int = 1, entity_category: str = cv.UNDEFINED
) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="%",
        accuracy_decimals=accuracy_decimals,
        state_class="measurement",
        entity_category=entity_category,
    )


def _count_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(
        accuracy_decimals=0,
        state_class="total_increasing",
        entity_category=entity_category,
    )


def _hours_schema(*, entity_category: str = cv.UNDEFINED) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement="h",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class="total_increasing",
        entity_category=entity_category,
    )


_CODE_SCHEMA = _code_schema()
_TEMPERATURE_SCHEMA = _temperature_schema()
_PERCENT_SCHEMA = _percent_schema()
_COUNT_SCHEMA = _count_schema()
_HOURS_SCHEMA = _hours_schema()


# Only for 1:1 ids (one sensor per conversation) -- ids that share a conversation with other
# entities get their interval from a hub-level option instead (see opentherm42/__init__.py's
# GROUP_UPDATE_INTERVAL_OPTIONS), so this must never be applied to a schema shared with one of
# those (e.g. _percent_schema() is also used by the 1:N MINIMUM_MODULATION_LEVEL sensor -- only
# wrap the specific 1:1 dict entries below, never a shared _*_SCHEMA constant itself).
def _with_update_interval(schema: cv.Schema) -> cv.Schema:
    return schema.extend(
        {
            cv.Required(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
        }
    )


TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 5 LB: OEM fault code (0..255) -- an OEM-specific fault/error code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 72 LB: OEM fault code ventilation/heat-recovery (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (0..255). 1:1 (HB is entirely
    # reserved, no other entity shares this conversation), unlike OEM_FAULT_CODE/
    # _VENTILATION_HEAT_RECOVERY above, which are part of the FAULT_FLAGS/VENTILATION_FAULT_FLAGS
    # groups -- see opentherm42/__init__.py's GROUP_UPDATE_INTERVAL_OPTIONS.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE: _with_update_interval(
        _CODE_SCHEMA
    ),
    # §5.3.1 Class 1, ID 115: OEM diagnostic code (0..65535) -- an OEM-specific diagnostic/service code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE: _with_update_interval(
        _CODE_SCHEMA
    ),
    # §5.3.1 Class 1, ID 73: OEM diagnostic code ventilation/heat-recovery (0..65535).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY: _with_update_interval(
        _CODE_SCHEMA
    ),
    # §5.3.2 Class 2, ID 3 LB: Boiler MemberID code (0..255) -- identifies the boiler's manufacturer.
    CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 125: OpenTherm protocol version implemented by the boiler.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER: _with_update_interval(
        _version_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.2 Class 2, ID 127 HB: Boiler product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 127 LB: Boiler product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 74 LB: MemberID code ventilation/heat-recovery (0..255).
    CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 75: OpenTherm protocol version implemented by the ventilation/heat-recovery system.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY: _with_update_interval(
        _version_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.2 Class 2, ID 76 HB: Ventilation/heat-recovery product version number and type: product type.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 76 LB: Ventilation/heat-recovery product version number and type: product version.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 103 LB: Solar Storage member ID (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 104 HB: Solar Storage product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.2 Class 2, ID 104 LB: Solar Storage product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.3 Class 3, ID 4 LB: Req-Response-Code of the most recent remote request
    # (0..127 = request refused, 128..255 = request accepted). Feedback for the Class 3 remote-request
    # buttons, which are all technician/installer actions (entity_category CONFIG) -- not a value
    # anyone watches day-to-day, so DIAGNOSTIC.
    CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE: _code_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.4 Class 4, ID 17: Relative Modulation Level (0..100%). 0% = minimum modulation level,
    # 100% = maximum modulation level.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL: _with_update_interval(
        _PERCENT_SCHEMA
    ),
    # §5.3.4 Class 4, ID 18: CH water pressure -- water pressure of the boiler CH circuit (bar, 0..5).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_WATER_PRESSURE: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement="bar",
            accuracy_decimals=2,
            device_class="pressure",
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 19: DHW flow rate -- water flow rate through the DHW circuit (l/min, 0..16).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_FLOW_RATE: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement="L/min",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 25: Boiler water temp. -- flow water temperature from the boiler (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_WATER_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 26: DHW temperature -- domestic hot water temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 28: Return water temperature to the boiler (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 29: Solar storage temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 30: Solar collector temperature (degrees C, -40..250). Unlike most temperatures
    # here this is a plain signed integer (s16), not f8.8, but it's still displayed the same way.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE: _with_update_interval(
        _temperature_schema(accuracy_decimals=0)
    ),
    # §5.3.4 Class 4, ID 31: Flow temperature CH2 -- flow water temperature of the 2nd CH circuit (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLOW_TEMPERATURE_CH2: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 32: DHW2 temperature -- domestic hot water temperature 2 (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW2_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 33: Exhaust temperature (degrees C, -40..500, s16 -- see ID 30's note).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_TEMPERATURE: _with_update_interval(
        _temperature_schema(accuracy_decimals=0)
    ),
    # §5.3.4 Class 4, ID 34: Boiler heat exchanger temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_HEAT_EXCHANGER_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 35 HB: Actual boiler fan speed Setpoint, wire value in Hz (RPM/60) converted to
    # RPM in hub.cpp -- 0..255 Hz -> 0..15300 RPM. No device_class: HA's number/sensor device classes
    # don't accept RPM as a valid unit (same reasoning as ids 84/85 below).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_SETPOINT: sensor.sensor_schema(
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 35 LB: Actual boiler fan speed, same Hz (RPM/60) -> RPM conversion as the
    # Setpoint above.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED: sensor.sensor_schema(
        unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        accuracy_decimals=0,
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 36: Flame current -- electrical current through the burner flame (uA, 0..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLAME_CURRENT: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement="µA",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_CURRENT,
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 77 LB: Relative ventilation (0-100%). 0% is minimum set value, 100% is
    # maximum. Wire value is a plain u8, not f8.8 like ID 17's Relative Modulation Level, so no
    # fractional part is ever possible -- accuracy_decimals=0.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION: _with_update_interval(
        _percent_schema(accuracy_decimals=0)
    ),
    # §5.3.4 Class 4, ID 80: Supply inlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 81: Supply outlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 82: Exhaust inlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_INLET_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 83: Exhaust outlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_OUTLET_TEMPERATURE: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.4 Class 4, ID 84: Actual exhaust fan speed (rpm, 0..6000).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_EXHAUST_FAN_SPEED: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=0,
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 85: Actual inlet fan speed (rpm, 0..6000).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_INLET_FAN_SPEED: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=0,
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 96: Number of hours the boiler is in Cooling Mode. Reset by zero is optional
    # for the boiler -- this component only reads it. A usage statistic, not component wear/service
    # data, so primary.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS: _with_update_interval(
        _HOURS_SCHEMA
    ),
    # §5.3.4 Class 4, ID 97: Number of Power Cycles of the boiler (wake-up after Reset). A reliability/
    # troubleshooting indicator (unexpected reboots), so DIAGNOSTIC.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 109: Number of starts of the electricity producer. A production statistic,
    # so primary.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS: _with_update_interval(
        _COUNT_SCHEMA
    ),
    # §5.3.4 Class 4, ID 110: Number of hours the electricity producer is in operation. A production
    # statistic, so primary.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS: _with_update_interval(
        _HOURS_SCHEMA
    ),
    # §5.3.4 Class 4, ID 111: Current electricity production (W).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCTION: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement="W",
            accuracy_decimals=0,
            device_class="power",
            state_class="measurement",
        )
    ),
    # §5.3.4 Class 4, ID 112: Cumulative Electricity production (kWh).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION: _with_update_interval(
        sensor.sensor_schema(
            unit_of_measurement="kWh",
            accuracy_decimals=0,
            device_class="energy",
            state_class="total_increasing",
        )
    ),
    # §5.3.4 Class 4, IDs 113/114/116-123: lifetime wear/reliability counters for an internal moving
    # part (burner ignitions, CH/DHW pump starts, unsuccessful starts, flame-signal faults) -- used
    # for service scheduling and troubleshooting, not day-to-day monitoring, so DIAGNOSTIC.
    #
    # §5.3.4 Class 4, ID 113: Number of un-successful burner starts.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 114: Number of times the flame signal was too low.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 116: Successful Burner starts.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 117: Number of starts of the CH pump.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 118: Number of starts of the DHW pump/valve.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 119: Number of starts of the burner in DHW mode.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS: _with_update_interval(
        _count_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 120: Number of hours the burner is in operation (i.e. flame on).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS: _with_update_interval(
        _hours_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 121: Number of hours the CH pump has been running.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS: _with_update_interval(
        _hours_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 122: Number of hours the DHW pump has been running or the DHW valve has been open.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS: _with_update_interval(
        _hours_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.4 Class 4, ID 123: Number of hours the burner is in operation during DHW mode.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS: _with_update_interval(
        _hours_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.5 Class 5, IDs 48/49: fixed installation-time adjustment bounds for the DHW Setpoint / max
    # CH water Setpoint, not something the user watches change -- DIAGNOSTIC.
    #
    # §5.3.5 Class 5, ID 48 HB: DHWsetp upp-bound -- upper bound for adjustment of DHW setp
    # (degrees C). Wire value is a plain s8, not f8.8 like ID 56's DHW Setpoint itself, so no
    # fractional part is ever possible -- accuracy_decimals=0.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND: _temperature_schema(
        accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.5 Class 5, ID 48 LB: DHWsetp low-bound -- lower bound for adjustment of DHW setp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND: _temperature_schema(
        accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.5 Class 5, ID 49 HB: max CHsetp upp-bound -- upper bound for adjustment of maxCHsetp
    # (degrees C), same plain-s8 reasoning as ID 48 above.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND: _temperature_schema(
        accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.5 Class 5, ID 49 LB: max CHsetp low-bnd -- lower bound for adjustment of maxCHsetp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND: _temperature_schema(
        accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.6 Class 6, IDs 10/88/105: static capability counts (how many parameters the boiler/
    # ventilation/Solar Storage system supports), not something that changes -- DIAGNOSTIC.
    #
    # §5.3.6 Class 6, ID 10 HB: Number of transparent-boiler-parameter's supported by the boiler.
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.6 Class 6, ID 88 HB: Number of TSPs supported by the ventilation/heat-recovery system.
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_VENTILATION_HEAT_RECOVERY: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.6 Class 6, ID 105 HB: Number of TSPs supported by the Solar Storage.
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_SOLAR_STORAGE: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.7 Class 7, IDs 12/90/107: static capability counts (fault history buffer size), same
    # reasoning as Class 6's TSP counts above -- DIAGNOSTIC.
    #
    # §5.3.7 Class 7, ID 12 HB: Size of the fault history buffer supported by the boiler.
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.7 Class 7, ID 90 HB: Size of the fault history buffer for the ventilation/heat-recovery system.
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.7 Class 7, ID 107 HB: Size of the fault history buffer for the Solar Storage.
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_SOLAR_STORAGE: _with_update_interval(
        _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    ),
    # §5.3.8.2 Class 8, ID 15 HB: Maximum boiler capacity (0..255 kW) -- a fixed hardware rating, not
    # a live power reading, so DIAGNOSTIC despite the "power" device_class matching its kW unit.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_BOILER_CAPACITY: sensor.sensor_schema(
        unit_of_measurement="kW",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_POWER,
        state_class="measurement",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.8.2 Class 8, ID 15 LB: Minimum modulation level (0..100%) -- a fixed installation rating,
    # same reasoning as Maximum boiler capacity above, so DIAGNOSTIC. Wire value is a plain u8
    # (same "Max-Capacity/Min-Mod-Level u8/u8" pair as ID 15 HB), so accuracy_decimals=0.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MINIMUM_MODULATION_LEVEL: _percent_schema(
        accuracy_decimals=0, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
    # §5.3.8.3 Class 8, ID 9: Remote Override Room Setpoint (degrees C, 0..30).
    # 0 = No override, 1..30 = Remote override room Setpoint. Unlike the fixed capability/rating
    # values just above, this is the setpoint actually in effect right now, so primary.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
    # §5.3.8.3 Class 8, ID 39: Remote Override Room Setpoint 2 (degrees C, 0..30), same encoding
    # and reasoning as ID 9 above.
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_2: _with_update_interval(
        _TEMPERATURE_SCHEMA
    ),
}

# Every 1:1 id above dispatched through hub.cpp's generic SIMPLE_SENSORS table -- update_interval
# for these routes through the single generic hub.set_simple_sensor_update_interval(id, ms), keyed
# by data-id rather than one named setter per marker (unlike every other platform here). Excludes
# OEM_DIAGNOSTIC_CODE/_VENTILATION_HEAT_RECOVERY, which have bespoke (non-SIMPLE_SENSORS) handling
# in hub.cpp and so get their own individually-named setters instead -- see to_code() below.
SIMPLE_SENSOR_DATA_IDS: dict[str, int] = {
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL: 17,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_WATER_PRESSURE: 18,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_FLOW_RATE: 19,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_WATER_TEMPERATURE: 25,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_TEMPERATURE: 26,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE: 28,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE: 29,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE: 30,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLOW_TEMPERATURE_CH2: 31,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW2_TEMPERATURE: 32,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_TEMPERATURE: 33,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_HEAT_EXCHANGER_TEMPERATURE: 34,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLAME_CURRENT: 36,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION: 77,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE: 80,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE: 81,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_INLET_TEMPERATURE: 82,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_OUTLET_TEMPERATURE: 83,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_EXHAUST_FAN_SPEED: 84,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_INLET_FAN_SPEED: 85,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS: 96,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES: 97,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS: 109,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS: 110,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCTION: 111,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION: 112,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS: 113,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW: 114,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS: 116,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS: 117,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS: 118,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS: 119,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS: 120,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS: 121,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS: 122,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS: 123,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS: 10,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_VENTILATION_HEAT_RECOVERY: 88,
    CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_SOLAR_STORAGE: 105,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER: 12,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY: 90,
    CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_SOLAR_STORAGE: 107,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT: 9,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_2: 39,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER: 125,
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY: 75,
}

# §5.3.7 Class 7, IDs 13/91/108: one list of user-named fault-history-buffer slots per family, keyed
# by which data-id reads that family's fault history.
FHB_FAMILY_DATA_IDS: dict[str, int] = {
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER: 13,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY: 91,
    CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_SOLAR_STORAGE: 108,
}

# Stored fault-history entries are diagnostic/troubleshooting information by nature, matching Home
# Assistant's own example of a diagnostic entity.
FHB_ENTRY_SCHEMA = _code_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC).extend(
    {
        # FHB-index: which of the boiler's (opaque, manufacturer-specific) fault history entries this
        # slot reads.
        cv.Required(CONF_INDEX): cv.int_range(min=0, max=255),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
        **{
            cv.Optional(marker, default=[]): cv.ensure_list(FHB_ENTRY_SCHEMA)
            for marker in FHB_FAMILY_DATA_IDS
        },
    }
)


FINAL_VALIDATE_SCHEMA = cv.All(
    validate_requires_hub_option(
        [CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE],
        CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_UPDATE_INTERVAL,
        "the OEM fault code (id=5) sensor",
    ),
    validate_requires_hub_option(
        [CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY],
        CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
        "the OEM fault code ventilation/heat-recovery (id=72) sensor",
    ),
    validate_requires_hub_option(
        [CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY],
        CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_UPDATE_INTERVAL,
        "the member ID code ventilation/heat-recovery (id=74) sensor",
    ),
    validate_requires_hub_option(
        [CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID],
        CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_UPDATE_INTERVAL,
        "the solar storage member ID (id=103) sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
            CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
        ],
        CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
        "either id=127 boiler product version sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
            CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
        ],
        CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
        "either id=76 ventilation/heat-recovery product version sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE,
            CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION,
        ],
        CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_UPDATE_INTERVAL,
        "either id=104 solar storage product version sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_SETPOINT,
            CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED,
        ],
        CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_UPDATE_INTERVAL,
        "either id=35 boiler fan speed sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND,
            CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND,
        ],
        CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPDATE_INTERVAL,
        "either id=48 DHWsetp bound sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND,
            CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND,
        ],
        CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPDATE_INTERVAL,
        "either id=49 max CHsetp bound sensor",
    ),
    validate_requires_hub_option(
        [
            CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_BOILER_CAPACITY,
            CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MINIMUM_MODULATION_LEVEL,
        ],
        CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAX_CAPACITY_MIN_MOD_LEVEL_UPDATE_INTERVAL,
        "either id=15 boiler capacity/modulation level sensor",
    ),
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await sensor.new_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_sensor")(var))
            if (interval := marker_config.get(CONF_UPDATE_INTERVAL)) is not None:
                if marker in SIMPLE_SENSOR_DATA_IDS:
                    cg.add(
                        hub.set_simple_sensor_update_interval(
                            SIMPLE_SENSOR_DATA_IDS[marker], interval
                        )
                    )
                else:
                    # OEM_DIAGNOSTIC_CODE/_VENTILATION_HEAT_RECOVERY and OEM_FAULT_CODE_SOLAR_STORAGE --
                    # bespoke (non-SIMPLE_SENSORS) handling in hub.cpp, so each gets its own
                    # individually-named setter instead.
                    cg.add(getattr(hub, f"set_{marker}_update_interval")(interval))

    for marker, data_id in FHB_FAMILY_DATA_IDS.items():
        for entry in config[marker]:
            var = await sensor.new_sensor(entry)
            cg.add(hub.add_fhb_slot(data_id, entry[CONF_INDEX], var))
