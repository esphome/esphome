import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import OpenTherm42Hub, validate_requires_time_id
from ..const import (
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CH2_PRESENT,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CONTROL_TYPE,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_COOLING_CONFIG,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_CONFIG,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_PRESENT,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_HEAT_COOL_MODE_CONTROL,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_MASTER_LOW_OFF_AND_PUMP_CONTROL_FUNCTION,
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_REMOTE_WATER_FILLING_FUNCTION,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_BYPASS,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SPEED_CONTROL,
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SYSTEM_TYPE,
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_SYSTEM_TYPE,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_AIR_PRESS_FAULT,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_GAS_FLAME_FAULT,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOCKOUT_RESET,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOW_WATER_PRESS,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_SERVICE_REQUEST,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_EXHAUST_FAN_FAULT,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_FROST_PROTECTION,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_INLET_FAN_FAULT,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_SERVICE_REQUEST,
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_WATER_OVER_TEMP,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH2_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_COOLING_STATUS,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DHW_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DIAGNOSTIC_SERVICE_INDICATION,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_ELECTRICITY_PRODUCTION,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FAULT_INDICATION,
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FLAME_STATUS,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_FAULT_INDICATION,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_AUTOMATIC_STATUS,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_STATUS,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_DIAGNOSTIC_INDICATION,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FAULT_INDICATION,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_STATUS,
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_VENTILATION_MODE,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_MANUAL_CHANGE_PRIORITY,
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_PROGRAM_CHANGE_PRIORITY,
    CONF_OPENTHERM42_ID,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_DHW_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_MAX_CHSETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_DHW_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_MAX_CHSETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TIME_SYNCHRONIZED,
)

# All of Class 1's binary_sensors are boiler-reported status bits: on a failed conversation, every
# configured bit here must show unknown rather than keep a stale reading -- see the hub's
# FlagReadBits::invalidate().
#
# Each entry maps to (device_class, entity_category). Class 1 ("Control and Status Information")
# bits are live, continuously-refreshed status/fault readouts -- primary (entity_category unset).
# Class 2 ("Configuration Information") bits, the Class 5 remote-parameter transfer-enable/
# read-write flags, and the Class 8 ID 100 remote-override-function bits are static capability
# flags the boiler reports once (the spec recommends reading them before normal operation begins)
# and never something the user watches change day-to-day -- DIAGNOSTIC.
DESCRIPTIONS: dict[str, tuple[str | None, str | None]] = {
    # §5.3.1 Class 1, ID 0 LB bit 0: fault indication [ no fault, fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FAULT_INDICATION: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 0 LB bit 1: CH mode [ CH not active, CH active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH_MODE: ("running", None),
    # §5.3.1 Class 1, ID 0 LB bit 2: DHW mode [ DHW not active, DHW active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DHW_MODE: ("running", None),
    # §5.3.1 Class 1, ID 0 LB bit 3: Flame status [ flame off, flame on ] -- "running" (is the
    # burner active right now), not "heat" (which HA defines as an abnormal/unsafe temperature
    # condition -- see Water over-temp below).
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FLAME_STATUS: ("running", None),
    # §5.3.1 Class 1, ID 0 LB bit 4: Cooling status [ cooling mode not active, cooling mode active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_COOLING_STATUS: ("running", None),
    # §5.3.1 Class 1, ID 0 LB bit 5: CH2 mode [ CH2 not active, CH2 active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH2_MODE: ("running", None),
    # §5.3.1 Class 1, ID 0 LB bit 6: diagnostic/service indication [no diagnostic/service, diagnostic/service event]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DIAGNOSTIC_SERVICE_INDICATION: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 0 LB bit 7: Electricity production [off, on]
    CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_ELECTRICITY_PRODUCTION: (
        "running",
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 0: Fault indication [ no fault, fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FAULT_INDICATION: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 1: Ventilation mode [ not active, active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_VENTILATION_MODE: (
        "running",
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 2: Bypass status [ closed, open ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_STATUS: (
        "opening",
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 3: Bypass automatic status [ manual, automatic ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_AUTOMATIC_STATUS: (
        None,
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 4: Free ventilation status [ not active, active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_STATUS: (
        "running",
        None,
    ),
    # §5.3.1 Class 1, ID 70 LB bit 6: Diagnostic indication [ no diagnostics, diagnostic event ]
    CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_DIAGNOSTIC_INDICATION: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 0: Service request [ service not req'd, service required ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_SERVICE_REQUEST: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 1: Lockout-reset [ remote reset disabled, remote reset enabled ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOCKOUT_RESET: (
        None,
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 2: Low water press [ no WP fault, water pressure fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOW_WATER_PRESS: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 3: Gas/flame fault [ no G/F fault, gas/flame fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_GAS_FLAME_FAULT: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 4: Air press fault [ no AP fault, air pressure fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_AIR_PRESS_FAULT: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 5 HB bit 5: Water over-temp [ no OvT fault, over-temperature fault ] --
    # "heat" (HA: on=hot/abnormal, off=normal) is a more precise match than generic "problem".
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_WATER_OVER_TEMP: (
        "heat",
        None,
    ),
    # §5.3.1 Class 1, ID 72 HB bit 0: Service request, ventilation/heat-recovery
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_SERVICE_REQUEST: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 72 HB bit 1: Exhaust fan fault [ no fault, fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_EXHAUST_FAN_FAULT: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 72 HB bit 2: Inlet fan fault [ no fault, fault ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_INLET_FAN_FAULT: (
        "problem",
        None,
    ),
    # §5.3.1 Class 1, ID 72 HB bit 3: Frost protection [ not active, active ]
    CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_FROST_PROTECTION: (
        "cold",
        None,
    ),
    # §5.3.1 Class 1, ID 101 LB bit 0: fault indication
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_FAULT_INDICATION: (
        "problem",
        None,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 0: DHW present [ dhw not present, dhw is present ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_PRESENT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 1: Control type [ modulating, on/off ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CONTROL_TYPE: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 2: Cooling config [ cooling not supported, cooling supported ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_COOLING_CONFIG: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 3: DHW config [ instantaneous or not-specified, storage tank ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_CONFIG: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 4: Master low-off&pump control function [ allowed, not allowed ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_MASTER_LOW_OFF_AND_PUMP_CONTROL_FUNCTION: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 5: CH2 present [ CH2 not present, CH2 present ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CH2_PRESENT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 6: Remote water filling function [ available or unknown, not available ].
    # Unknown for applications with protocol version 2.2 or older.
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_REMOTE_WATER_FILLING_FUNCTION: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 3 HB bit 7: Heat/cool mode control [ switching done by master, switching done by boiler ]
    CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_HEAT_COOL_MODE_CONTROL: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 74 HB bit 0: System type [ 0 = central exhaust ventilation, 1 = heat-recovery ventilation ]
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SYSTEM_TYPE: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 74 HB bit 1: Bypass [ not present, present ]
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_BYPASS: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 74 HB bit 2: Speed control [ 3-speed, variable ]
    CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SPEED_CONTROL: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.2 Class 2, ID 103 HB bit 0: Solar Storage configuration: system type
    # [ 0 = DHW preheat system, 1 = DHW parallel system ]
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_SYSTEM_TYPE: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 6 HB bit 0: DHW Setpoint [ transfer disabled, transfer enabled ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_DHW_SETPOINT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 6 HB bit 1: max CHsetpoint [ transfer disabled, transfer enabled ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_MAX_CHSETPOINT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 6 LB bit 0: DHW Setpoint [ read-only, read/write ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_DHW_SETPOINT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 6 LB bit 1: max CHsetpoint [ read-only, read/write ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_MAX_CHSETPOINT: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 86 HB bit 0: Nominal ventilation value [ transfer disabled, transfer enabled ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.5 Class 5, ID 86 LB bit 0: Nominal ventilation value [ read-only, read/write ]
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.8.3 Class 8, ID 100 LB bit 0: Manual change priority
    # [ disable overruling remote Setpoint by manual Setpoint change, enable overruling remote Setpoint
    # by manual Setpoint change ]
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_MANUAL_CHANGE_PRIORITY: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.8.3 Class 8, ID 100 LB bit 1: Program change priority
    # [ disable overruling remote Setpoint by program Setpoint change, enable overruling remote Setpoint
    # by program Setpoint change ]
    CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_PROGRAM_CHANGE_PRIORITY: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    # §5.3.4 Class 4, IDs 20/21/22: synthetic diagnostic entity (not a real spec data-id) -- true only
    # once the most recent Day-of-week/Time, Date and Year writes described by opentherm42/__init__.py's
    # time_id option all succeeded. No device_class fits "sync succeeded" semantics (HA's closest,
    # "problem", is inverted: on would mean a problem, not success), so the entity's name alone conveys
    # the state. A troubleshooting indicator, not something watched day-to-day, so DIAGNOSTIC.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TIME_SYNCHRONIZED: (
        None,
        ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

TYPES: dict[str, cv.Schema] = {
    marker: binary_sensor.binary_sensor_schema(
        device_class=device_class or cv.UNDEFINED,
        entity_category=entity_category or cv.UNDEFINED,
    )
    for marker, (device_class, entity_category) in DESCRIPTIONS.items()
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)

FINAL_VALIDATE_SCHEMA = validate_requires_time_id(
    CONF_SENSOR_AND_INFORMATIONAL_DATA_TIME_SYNCHRONIZED
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await binary_sensor.new_binary_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_binary_sensor")(var))
