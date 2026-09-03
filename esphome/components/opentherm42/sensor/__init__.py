import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import OpenTherm42Hub
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
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE,
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE,
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS,
    CONF_OPENTHERM42_ID,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND,
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE,
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
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL,
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
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE,
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE,
)

# All of these sensors are boiler-reported values: on a failed conversation, every configured sensor
# here must show unknown rather than keep a stale reading.
_CODE_SCHEMA = sensor.sensor_schema(accuracy_decimals=0)
# §5.1: OpenTherm protocol versions are f8.8 (e.g. 2.2, 4.2) -- two decimals is enough to show them exactly.
_VERSION_SCHEMA = sensor.sensor_schema(accuracy_decimals=2)
_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="°C",
    accuracy_decimals=2,
    device_class="temperature",
    state_class="measurement",
)
_PERCENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="%", accuracy_decimals=1, state_class="measurement"
)
_COUNT_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0, state_class="total_increasing"
)
_HOURS_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="h", accuracy_decimals=0, state_class="total_increasing"
)

TYPES: dict[str, cv.Schema] = {
    # §5.3.1 Class 1, ID 5 LB: OEM fault code (0..255) -- an OEM-specific fault/error code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 72 LB: OEM fault code ventilation/heat-recovery (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (0..255).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 115: OEM diagnostic code (0..65535) -- an OEM-specific diagnostic/service code.
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 73: OEM diagnostic code ventilation/heat-recovery (0..65535).
    CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 HB bits 2,1,0: Master Solar Storage status: Solar mode.
    # 0=off, 1=DHW eco, 2=DHW comfort, 3=DHW single boost, 4=DHW continuous boost.
    CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 LB bits 3,2,1: Solar Storage mode and status: Solar mode.
    # 0=off, 1=DHW eco, 2=DHW comfort, 3=DHW single boost, 4=DHW continuous boost.
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE: _CODE_SCHEMA,
    # §5.3.1 Class 1, ID 101 LB bits 5,4: Solar Storage mode and status: Solar status.
    # 0=standby, 1=loading of solar storage tank by the sun, 2=loading by the boiler, 3=anti-legionella mode active.
    CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 3 LB: Boiler MemberID code (0..255) -- identifies the boiler's manufacturer.
    CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 125: OpenTherm protocol version implemented by the boiler.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER: _VERSION_SCHEMA,
    # §5.3.2 Class 2, ID 127 HB: Boiler product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 127 LB: Boiler product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 74 LB: MemberID code ventilation/heat-recovery (0..255).
    CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 75: OpenTherm protocol version implemented by the ventilation/heat-recovery system.
    CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY: _VERSION_SCHEMA,
    # §5.3.2 Class 2, ID 76 HB: Ventilation/heat-recovery product version number and type: product type.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: (
        _CODE_SCHEMA
    ),
    # §5.3.2 Class 2, ID 76 LB: Ventilation/heat-recovery product version number and type: product version.
    CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: (
        _CODE_SCHEMA
    ),
    # §5.3.2 Class 2, ID 103 LB: Solar Storage member ID (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 104 HB: Solar Storage product version number and type: product type (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE: _CODE_SCHEMA,
    # §5.3.2 Class 2, ID 104 LB: Solar Storage product version number and type: product version (0..255).
    CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION: _CODE_SCHEMA,
    # §5.3.3 Class 3, ID 4 LB: Req-Response-Code of the most recent remote request
    # (0..127 = request refused, 128..255 = request accepted).
    CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE: _CODE_SCHEMA,
    # §5.3.4 Class 4, ID 17: Relative Modulation Level (0..100%). 0% = minimum modulation level,
    # 100% = maximum modulation level.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL: _PERCENT_SCHEMA,
    # §5.3.4 Class 4, ID 18: CH water pressure -- water pressure of the boiler CH circuit (bar, 0..5).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_WATER_PRESSURE: sensor.sensor_schema(
        unit_of_measurement="bar",
        accuracy_decimals=2,
        device_class="pressure",
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 19: DHW flow rate -- water flow rate through the DHW circuit (l/min, 0..16).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_FLOW_RATE: sensor.sensor_schema(
        unit_of_measurement="L/min", accuracy_decimals=2, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 25: Boiler water temp. -- flow water temperature from the boiler (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_WATER_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 26: DHW temperature -- domestic hot water temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 28: Return water temperature to the boiler (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 29: Solar storage temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 30: Solar collector temperature (degrees C, -40..250). Unlike most temperatures
    # here this is a plain signed integer (s16), not f8.8, but it's still displayed the same way.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement="°C",
        accuracy_decimals=0,
        device_class="temperature",
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 31: Flow temperature CH2 -- flow water temperature of the 2nd CH circuit (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLOW_TEMPERATURE_CH2: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 32: DHW2 temperature -- domestic hot water temperature 2 (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW2_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 33: Exhaust temperature (degrees C, -40..500, s16 -- see ID 30's note).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement="°C",
        accuracy_decimals=0,
        device_class="temperature",
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 34: Boiler heat exchanger temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_HEAT_EXCHANGER_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 35 HB: Actual boiler fan speed Setpoint in Hz (RPM/60), 0..255.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_SETPOINT: sensor.sensor_schema(
        unit_of_measurement="Hz", accuracy_decimals=0, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 35 LB: Actual boiler fan speed in Hz (RPM/60), 0..255.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED: sensor.sensor_schema(
        unit_of_measurement="Hz", accuracy_decimals=0, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 36: Flame current -- electrical current through the burner flame (uA, 0..127).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_FLAME_CURRENT: sensor.sensor_schema(
        unit_of_measurement="µA", accuracy_decimals=2, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 77 LB: Relative ventilation (0-100%). 0% is minimum set value, 100% is maximum.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION: _PERCENT_SCHEMA,
    # §5.3.4 Class 4, ID 80: Supply inlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 81: Supply outlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 82: Exhaust inlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_INLET_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 83: Exhaust outlet temperature (degrees C).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_OUTLET_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 84: Actual exhaust fan speed (rpm, 0..6000).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_EXHAUST_FAN_SPEED: sensor.sensor_schema(
        unit_of_measurement="rpm", accuracy_decimals=0, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 85: Actual inlet fan speed (rpm, 0..6000).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_INLET_FAN_SPEED: sensor.sensor_schema(
        unit_of_measurement="rpm", accuracy_decimals=0, state_class="measurement"
    ),
    # §5.3.4 Class 4, ID 96: Number of hours the boiler is in Cooling Mode. Reset by zero is optional
    # for the boiler -- this component only reads it.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 97: Number of Power Cycles of the boiler (wake-up after Reset).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 109: Number of starts of the electricity producer.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 110: Number of hours the electricity producer is in operation.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 111: Current electricity production (W).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCTION: sensor.sensor_schema(
        unit_of_measurement="W",
        accuracy_decimals=0,
        device_class="power",
        state_class="measurement",
    ),
    # §5.3.4 Class 4, ID 112: Cumulative Electricity production (kWh).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION: sensor.sensor_schema(
        unit_of_measurement="kWh",
        accuracy_decimals=0,
        device_class="energy",
        state_class="total_increasing",
    ),
    # §5.3.4 Class 4, ID 113: Number of un-successful burner starts.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 114: Number of times the flame signal was too low.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 116: Successful Burner starts.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 117: Number of starts of the CH pump.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 118: Number of starts of the DHW pump/valve.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 119: Number of starts of the burner in DHW mode.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS: _COUNT_SCHEMA,
    # §5.3.4 Class 4, ID 120: Number of hours the burner is in operation (i.e. flame on).
    CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 121: Number of hours the CH pump has been running.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 122: Number of hours the DHW pump has been running or the DHW valve has been open.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 123: Number of hours the burner is in operation during DHW mode.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS: _HOURS_SCHEMA,
    # §5.3.4 Class 4, ID 27: Outside temperature (degrees C), read from the boiler. Only used if the
    # number platform's outside_temperature_set is NOT configured -- see that option's description.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE: _TEMPERATURE_SCHEMA,
    # §5.3.4 Class 4, ID 38: Relative Humidity (0..100%), read from the boiler. Only used if the number
    # platform's relative_humidity_set is NOT configured.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY: _PERCENT_SCHEMA,
    # §5.3.4 Class 4, ID 78 LB: Relative humidity exhaust air (0..100%), read from the boiler. Only used
    # if the number platform's relative_humidity_exhaust_air_set is NOT configured.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR: _PERCENT_SCHEMA,
    # §5.3.4 Class 4, ID 79: CO2 level exhaust air (0..2000 ppm), read from the boiler. Only used if the
    # number platform's co2_level_set is NOT configured.
    CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL: sensor.sensor_schema(
        unit_of_measurement="ppm",
        accuracy_decimals=0,
        device_class="carbon_dioxide",
        state_class="measurement",
    ),
    # §5.3.5 Class 5, ID 48 HB: DHWsetp upp-bound -- upper bound for adjustment of DHW setp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 48 LB: DHWsetp low-bound -- lower bound for adjustment of DHW setp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 49 HB: max CHsetp upp-bound -- upper bound for adjustment of maxCHsetp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 49 LB: max CHsetp low-bnd -- lower bound for adjustment of maxCHsetp (degrees C).
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 56: DHW Setpoint -- domestic hot water temperature Setpoint (degrees C), read
    # from the boiler. Only used if the number platform's dhw_setpoint_set is NOT configured.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 57: max CH water Setpoint -- maximum allowable CH water Setpoint (degrees C),
    # read from the boiler. Only used if the number platform's max_ch_water_setpoint_set is NOT configured.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT: _TEMPERATURE_SCHEMA,
    # §5.3.5 Class 5, ID 87 HB: Nominal ventilation value (0-100%), read from the boiler. Only used if
    # the number platform's nominal_ventilation_value_set is NOT configured.
    CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE: _PERCENT_SCHEMA,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_OPENTHERM42_ID): cv.use_id(OpenTherm42Hub),
        **{cv.Optional(marker): schema for marker, schema in TYPES.items()},
    }
)


async def to_code(config: dict) -> None:
    hub = await cg.get_variable(config[CONF_OPENTHERM42_ID])
    for marker in TYPES:
        if (marker_config := config.get(marker)) is not None:
            var = await sensor.new_sensor(marker_config)
            cg.add(getattr(hub, f"set_{marker}_sensor")(var))
