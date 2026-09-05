# Config keys shared by opentherm42's platform files (switch/number/binary_sensor/sensor). Named
# after the OpenTherm Protocol Specification v4.2 section + field hierarchy, with "HB"/"LB" stripped
# and the spec's term for the boiler side of the conversation replaced by "boiler" (see CLAUDE.md's
# C++ enumerator-naming rule for why that term is avoided project-wide).

# Every platform file's schema uses this to reference the opentherm42: hub.
CONF_OPENTHERM42_ID = "opentherm42_id"

# §5.3.1 Class 1, ID 0 HB: Master status -- one switch per control bit.
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH_ENABLE = (
    "control_and_status_information_master_status_ch_enable"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_ENABLE = (
    "control_and_status_information_master_status_dhw_enable"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_COOLING_ENABLE = (
    "control_and_status_information_master_status_cooling_enable"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_OTC_ACTIVE = (
    "control_and_status_information_master_status_otc_active"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_CH2_ENABLE = (
    "control_and_status_information_master_status_ch2_enable"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_SUMMER_WINTER_MODE = (
    "control_and_status_information_master_status_summer_winter_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_DHW_BLOCKING = (
    "control_and_status_information_master_status_dhw_blocking"
)

# §5.3.1 Class 1, ID 70 HB: Master status for ventilation/heat-recovery.
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_VENTILATION_ENABLE = "control_and_status_information_master_status_for_ventilation_heat_recovery_ventilation_enable"
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_POSITION = "control_and_status_information_master_status_for_ventilation_heat_recovery_bypass_position"
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_BYPASS_MODE = "control_and_status_information_master_status_for_ventilation_heat_recovery_bypass_mode"
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_STATUS_FOR_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_MODE = "control_and_status_information_master_status_for_ventilation_heat_recovery_free_ventilation_mode"

# §5.3.1 Class 1, IDs 1/8/71: numeric setpoints the master writes.
CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT = (
    "control_and_status_information_control_setpoint"
)
CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_2_TSETCH2 = (
    "control_and_status_information_control_setpoint_2_tsetch2"
)
CONF_CONTROL_AND_STATUS_INFORMATION_CONTROL_SETPOINT_VENTILATION_HEAT_RECOVERY = (
    "control_and_status_information_control_setpoint_ventilation_heat_recovery"
)

# §5.3.1 Class 1, ID 0 LB: Boiler status -- one binary_sensor per status bit.
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FAULT_INDICATION = (
    "control_and_status_information_boiler_status_fault_indication"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH_MODE = (
    "control_and_status_information_boiler_status_ch_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DHW_MODE = (
    "control_and_status_information_boiler_status_dhw_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_FLAME_STATUS = (
    "control_and_status_information_boiler_status_flame_status"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_COOLING_STATUS = (
    "control_and_status_information_boiler_status_cooling_status"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_CH2_MODE = (
    "control_and_status_information_boiler_status_ch2_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_DIAGNOSTIC_SERVICE_INDICATION = (
    "control_and_status_information_boiler_status_diagnostic_service_indication"
)
CONF_CONTROL_AND_STATUS_INFORMATION_BOILER_STATUS_ELECTRICITY_PRODUCTION = (
    "control_and_status_information_boiler_status_electricity_production"
)

# §5.3.1 Class 1, ID 70 LB: Status ventilation/heat-recovery.
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FAULT_INDICATION = "control_and_status_information_status_ventilation_heat_recovery_fault_indication"
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_VENTILATION_MODE = "control_and_status_information_status_ventilation_heat_recovery_ventilation_mode"
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_STATUS = (
    "control_and_status_information_status_ventilation_heat_recovery_bypass_status"
)
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_BYPASS_AUTOMATIC_STATUS = "control_and_status_information_status_ventilation_heat_recovery_bypass_automatic_status"
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_FREE_VENTILATION_STATUS = "control_and_status_information_status_ventilation_heat_recovery_free_ventilation_status"
CONF_CONTROL_AND_STATUS_INFORMATION_STATUS_VENTILATION_HEAT_RECOVERY_DIAGNOSTIC_INDICATION = "control_and_status_information_status_ventilation_heat_recovery_diagnostic_indication"

# §5.3.1 Class 1, ID 5 HB: Application-specific fault flags.
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_SERVICE_REQUEST = (
    "control_and_status_information_application_specific_fault_flags_service_request"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOCKOUT_RESET = (
    "control_and_status_information_application_specific_fault_flags_lockout_reset"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_LOW_WATER_PRESS = (
    "control_and_status_information_application_specific_fault_flags_low_water_press"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_GAS_FLAME_FAULT = (
    "control_and_status_information_application_specific_fault_flags_gas_flame_fault"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_AIR_PRESS_FAULT = (
    "control_and_status_information_application_specific_fault_flags_air_press_fault"
)
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_WATER_OVER_TEMP = (
    "control_and_status_information_application_specific_fault_flags_water_over_temp"
)
CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE = (
    "control_and_status_information_oem_fault_code"
)

# §5.3.1 Class 1, ID 72: Application-specific fault flags ventilation/heat-recovery.
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_SERVICE_REQUEST = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_service_request"
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_EXHAUST_FAN_FAULT = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_exhaust_fan_fault"
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_INLET_FAN_FAULT = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_inlet_fan_fault"
CONF_CONTROL_AND_STATUS_INFORMATION_APPLICATION_SPECIFIC_FAULT_FLAGS_VENTILATION_HEAT_RECOVERY_FROST_PROTECTION = "control_and_status_information_application_specific_fault_flags_ventilation_heat_recovery_frost_protection"
CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_VENTILATION_HEAT_RECOVERY = (
    "control_and_status_information_oem_fault_code_ventilation_heat_recovery"
)

# §5.3.1 Class 1, ID 101: Master Solar Storage status / Solar Storage mode and status.
CONF_CONTROL_AND_STATUS_INFORMATION_MASTER_SOLAR_STORAGE_STATUS_SOLAR_MODE = (
    "control_and_status_information_master_solar_storage_status_solar_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_FAULT_INDICATION = (
    "control_and_status_information_solar_storage_mode_and_status_fault_indication"
)
CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_MODE = (
    "control_and_status_information_solar_storage_mode_and_status_solar_mode"
)
CONF_CONTROL_AND_STATUS_INFORMATION_SOLAR_STORAGE_MODE_AND_STATUS_SOLAR_STATUS = (
    "control_and_status_information_solar_storage_mode_and_status_solar_status"
)

# §5.3.1 Class 1, ID 102 LB: OEM fault code Solar Storage (HB is entirely reserved -- no entity).
CONF_CONTROL_AND_STATUS_INFORMATION_OEM_FAULT_CODE_SOLAR_STORAGE = (
    "control_and_status_information_oem_fault_code_solar_storage"
)

# §5.3.1 Class 1, IDs 115/73: OEM diagnostic codes.
CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE = (
    "control_and_status_information_oem_diagnostic_code"
)
CONF_CONTROL_AND_STATUS_INFORMATION_OEM_DIAGNOSTIC_CODE_VENTILATION_HEAT_RECOVERY = (
    "control_and_status_information_oem_diagnostic_code_ventilation_heat_recovery"
)

# §5.3.2 Class 2, ID 3 HB: Boiler configuration -- what the boiler reports it supports.
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_PRESENT = (
    "configuration_information_boiler_configuration_dhw_present"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CONTROL_TYPE = (
    "configuration_information_boiler_configuration_control_type"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_COOLING_CONFIG = (
    "configuration_information_boiler_configuration_cooling_config"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_DHW_CONFIG = (
    "configuration_information_boiler_configuration_dhw_config"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_MASTER_LOW_OFF_AND_PUMP_CONTROL_FUNCTION = "configuration_information_boiler_configuration_master_low_off_and_pump_control_function"
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_CH2_PRESENT = (
    "configuration_information_boiler_configuration_ch2_present"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_REMOTE_WATER_FILLING_FUNCTION = (
    "configuration_information_boiler_configuration_remote_water_filling_function"
)
CONF_CONFIGURATION_INFORMATION_BOILER_CONFIGURATION_HEAT_COOL_MODE_CONTROL = (
    "configuration_information_boiler_configuration_heat_cool_mode_control"
)
CONF_CONFIGURATION_INFORMATION_BOILER_MEMBER_ID_CODE = (
    "configuration_information_boiler_member_id_code"
)

# §5.3.2 Class 2, ID 74: Configuration ventilation/heat-recovery.
CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SYSTEM_TYPE = (
    "configuration_information_configuration_ventilation_heat_recovery_system_type"
)
CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_BYPASS = (
    "configuration_information_configuration_ventilation_heat_recovery_bypass"
)
CONF_CONFIGURATION_INFORMATION_CONFIGURATION_VENTILATION_HEAT_RECOVERY_SPEED_CONTROL = (
    "configuration_information_configuration_ventilation_heat_recovery_speed_control"
)
CONF_CONFIGURATION_INFORMATION_MEMBER_ID_CODE_VENTILATION_HEAT_RECOVERY = (
    "configuration_information_member_id_code_ventilation_heat_recovery"
)

# §5.3.2 Class 2, ID 103 HB bit 0: Solar Storage configuration: system type.
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_CONFIGURATION_SYSTEM_TYPE = (
    "configuration_information_solar_storage_configuration_system_type"
)
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_MEMBER_ID = (
    "configuration_information_solar_storage_member_id"
)

# §5.3.2 Class 2, IDs 125/75: OpenTherm protocol version implemented by the boiler / ventilation unit.
CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_BOILER = (
    "configuration_information_opentherm_version_boiler"
)
CONF_CONFIGURATION_INFORMATION_OPENTHERM_VERSION_VENTILATION_HEAT_RECOVERY = (
    "configuration_information_opentherm_version_ventilation_heat_recovery"
)

# §5.3.2 Class 2, IDs 127/76/104: product type/version reported by the boiler / ventilation unit /
# solar storage unit.
CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE = (
    "configuration_information_boiler_product_version_number_and_type_product_type"
)
CONF_CONFIGURATION_INFORMATION_BOILER_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION = "configuration_information_boiler_product_version_number_and_type_product_version"
CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE = "configuration_information_ventilation_heat_recovery_product_version_number_and_type_product_type"
CONF_CONFIGURATION_INFORMATION_VENTILATION_HEAT_RECOVERY_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION = "configuration_information_ventilation_heat_recovery_product_version_number_and_type_product_version"
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_TYPE = "configuration_information_solar_storage_product_version_number_and_type_product_type"
CONF_CONFIGURATION_INFORMATION_SOLAR_STORAGE_PRODUCT_VERSION_NUMBER_AND_TYPE_PRODUCT_VERSION = "configuration_information_solar_storage_product_version_number_and_type_product_version"

# §5.3.2 Class 2, IDs 93/94/95: brand identification strings, read one character at a time.
CONF_CONFIGURATION_INFORMATION_BRAND = "configuration_information_brand"
CONF_CONFIGURATION_INFORMATION_BRAND_VERSION = "configuration_information_brand_version"
CONF_CONFIGURATION_INFORMATION_BRAND_SERIAL_NUMBER = (
    "configuration_information_brand_serial_number"
)

# §5.3.3 Class 3, ID 4 HB: one button per Request-Code value.
CONF_REMOTE_REQUEST_BACK_TO_NORMAL_OPERATION_MODE = (
    "remote_request_back_to_normal_operation_mode"
)
CONF_REMOTE_REQUEST_BOILER_LOCKOUT_RESET = "remote_request_boiler_lockout_reset"
CONF_REMOTE_REQUEST_CH_WATER_FILLING = "remote_request_ch_water_filling"
CONF_REMOTE_REQUEST_SERVICE_MODE_MAXIMUM_POWER = (
    "remote_request_service_mode_maximum_power"
)
CONF_REMOTE_REQUEST_SERVICE_MODE_MINIMUM_POWER = (
    "remote_request_service_mode_minimum_power"
)
CONF_REMOTE_REQUEST_SERVICE_MODE_SPARK_TEST = "remote_request_service_mode_spark_test"
CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MAXIMUM_SPEED = (
    "remote_request_service_mode_fan_maximum_speed"
)
CONF_REMOTE_REQUEST_SERVICE_MODE_FAN_MINIMUM_SPEED = (
    "remote_request_service_mode_fan_minimum_speed"
)
CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_CH = (
    "remote_request_service_mode_3_way_valve_to_ch"
)
CONF_REMOTE_REQUEST_SERVICE_MODE_3_WAY_VALVE_TO_DHW = (
    "remote_request_service_mode_3_way_valve_to_dhw"
)
CONF_REMOTE_REQUEST_RESET_SERVICE_REQUEST_FLAG = (
    "remote_request_reset_service_request_flag"
)
CONF_REMOTE_REQUEST_SERVICE_TEST_1 = "remote_request_service_test_1"
CONF_REMOTE_REQUEST_AUTOMATIC_HYDRONIC_AIR_PURGE = (
    "remote_request_automatic_hydronic_air_purge"
)

# §5.3.3 Class 3, ID 4 LB: the most recent Request-Response-Code (0..127 = refused, 128..255 = accepted).
CONF_REMOTE_REQUEST_LAST_RESPONSE_CODE = "remote_request_last_response_code"

# §5.3.4 Class 4: write-only numbers this master provides to the boiler.
CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT = (
    "sensor_and_informational_data_room_setpoint"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_SETPOINT_CH2 = (
    "sensor_and_informational_data_room_setpoint_ch2"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ROOM_TEMPERATURE = (
    "sensor_and_informational_data_room_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_TRCH2 = "sensor_and_informational_data_trch2"

# §5.3.4 Class 4, IDs 27/38/78/79: R/W ids -- if the "_set" number is configured this master writes
# its own value; otherwise, if the plain sensor is configured, this master reads the boiler's.
CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE = (
    "sensor_and_informational_data_outside_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_OUTSIDE_TEMPERATURE_SET = (
    "sensor_and_informational_data_outside_temperature_set"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY = (
    "sensor_and_informational_data_relative_humidity"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_SET = (
    "sensor_and_informational_data_relative_humidity_set"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR = (
    "sensor_and_informational_data_relative_humidity_exhaust_air"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_HUMIDITY_EXHAUST_AIR_SET = (
    "sensor_and_informational_data_relative_humidity_exhaust_air_set"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL = "sensor_and_informational_data_co2_level"
CONF_SENSOR_AND_INFORMATIONAL_DATA_CO2_LEVEL_SET = (
    "sensor_and_informational_data_co2_level_set"
)

# §5.3.4 Class 4: read-only sensors.
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_MODULATION_LEVEL = (
    "sensor_and_informational_data_relative_modulation_level"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_WATER_PRESSURE = (
    "sensor_and_informational_data_ch_water_pressure"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_FLOW_RATE = (
    "sensor_and_informational_data_dhw_flow_rate"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_WATER_TEMPERATURE = (
    "sensor_and_informational_data_boiler_water_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_TEMPERATURE = (
    "sensor_and_informational_data_dhw_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RETURN_WATER_TEMPERATURE = (
    "sensor_and_informational_data_return_water_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_STORAGE_TEMPERATURE = (
    "sensor_and_informational_data_solar_storage_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SOLAR_COLLECTOR_TEMPERATURE = (
    "sensor_and_informational_data_solar_collector_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_FLOW_TEMPERATURE_CH2 = (
    "sensor_and_informational_data_flow_temperature_ch2"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW2_TEMPERATURE = (
    "sensor_and_informational_data_dhw2_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_TEMPERATURE = (
    "sensor_and_informational_data_exhaust_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_HEAT_EXCHANGER_TEMPERATURE = (
    "sensor_and_informational_data_boiler_heat_exchanger_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED_SETPOINT = (
    "sensor_and_informational_data_boiler_fan_speed_setpoint"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BOILER_FAN_SPEED = (
    "sensor_and_informational_data_boiler_fan_speed"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_FLAME_CURRENT = (
    "sensor_and_informational_data_flame_current"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_RELATIVE_VENTILATION = (
    "sensor_and_informational_data_relative_ventilation"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_INLET_TEMPERATURE = (
    "sensor_and_informational_data_supply_inlet_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SUPPLY_OUTLET_TEMPERATURE = (
    "sensor_and_informational_data_supply_outlet_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_INLET_TEMPERATURE = (
    "sensor_and_informational_data_exhaust_inlet_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_EXHAUST_OUTLET_TEMPERATURE = (
    "sensor_and_informational_data_exhaust_outlet_temperature"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_EXHAUST_FAN_SPEED = (
    "sensor_and_informational_data_actual_exhaust_fan_speed"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ACTUAL_INLET_FAN_SPEED = (
    "sensor_and_informational_data_actual_inlet_fan_speed"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS = (
    "sensor_and_informational_data_cooling_operation_hours"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES = (
    "sensor_and_informational_data_power_cycles"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS = (
    "sensor_and_informational_data_electricity_producer_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS = (
    "sensor_and_informational_data_electricity_producer_hours"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCTION = (
    "sensor_and_informational_data_electricity_production"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION = (
    "sensor_and_informational_data_cumulative_electricity_production"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS = (
    "sensor_and_informational_data_number_of_unsuccessful_burner_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW = (
    "sensor_and_informational_data_number_of_times_flame_signal_too_low"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS = (
    "sensor_and_informational_data_successful_burner_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS = (
    "sensor_and_informational_data_ch_pump_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS = (
    "sensor_and_informational_data_dhw_pump_valve_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS = (
    "sensor_and_informational_data_dhw_burner_starts"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS = (
    "sensor_and_informational_data_burner_operation_hours"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS = (
    "sensor_and_informational_data_ch_pump_operation_hours"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS = (
    "sensor_and_informational_data_dhw_pump_valve_operation_hours"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS = (
    "sensor_and_informational_data_dhw_burner_operation_hours"
)

# §5.3.4 Class 4: reset-by-writing-zero buttons for the 14 counter/hour ids above (optional for the
# boiler to honor). ID 111 (Electricity production) is read-only and has no reset.
CONF_SENSOR_AND_INFORMATIONAL_DATA_COOLING_OPERATION_HOURS_RESET = (
    "sensor_and_informational_data_cooling_operation_hours_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_POWER_CYCLES_RESET = (
    "sensor_and_informational_data_power_cycles_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_STARTS_RESET = (
    "sensor_and_informational_data_electricity_producer_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_ELECTRICITY_PRODUCER_HOURS_RESET = (
    "sensor_and_informational_data_electricity_producer_hours_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CUMULATIVE_ELECTRICITY_PRODUCTION_RESET = (
    "sensor_and_informational_data_cumulative_electricity_production_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_UNSUCCESSFUL_BURNER_STARTS_RESET = (
    "sensor_and_informational_data_number_of_unsuccessful_burner_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_NUMBER_OF_TIMES_FLAME_SIGNAL_TOO_LOW_RESET = (
    "sensor_and_informational_data_number_of_times_flame_signal_too_low_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_SUCCESSFUL_BURNER_STARTS_RESET = (
    "sensor_and_informational_data_successful_burner_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_STARTS_RESET = (
    "sensor_and_informational_data_ch_pump_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_STARTS_RESET = (
    "sensor_and_informational_data_dhw_pump_valve_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_STARTS_RESET = (
    "sensor_and_informational_data_dhw_burner_starts_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_BURNER_OPERATION_HOURS_RESET = (
    "sensor_and_informational_data_burner_operation_hours_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_CH_PUMP_OPERATION_HOURS_RESET = (
    "sensor_and_informational_data_ch_pump_operation_hours_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_PUMP_VALVE_OPERATION_HOURS_RESET = (
    "sensor_and_informational_data_dhw_pump_valve_operation_hours_reset"
)
CONF_SENSOR_AND_INFORMATIONAL_DATA_DHW_BURNER_OPERATION_HOURS_RESET = (
    "sensor_and_informational_data_dhw_burner_operation_hours_reset"
)

# §5.3.5 Class 5, ID 6: Remote-parameter transfer-enable/read-write flags for DHW Setpoint / max CHsetpoint.
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_DHW_SETPOINT = (
    "pre_defined_remote_boiler_parameters_transfer_enable_flags_dhw_setpoint"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_MAX_CHSETPOINT = (
    "pre_defined_remote_boiler_parameters_transfer_enable_flags_max_chsetpoint"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_DHW_SETPOINT = (
    "pre_defined_remote_boiler_parameters_read_write_flags_dhw_setpoint"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_MAX_CHSETPOINT = (
    "pre_defined_remote_boiler_parameters_read_write_flags_max_chsetpoint"
)

# §5.3.5 Class 5, ID 86: same transfer-enable/read-write flags, for ventilation/heat-recovery's
# Nominal ventilation value.
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_TRANSFER_ENABLE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE = "pre_defined_remote_boiler_parameters_transfer_enable_flags_ventilation_heat_recovery_nominal_ventilation_value"
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_READ_WRITE_FLAGS_VENTILATION_HEAT_RECOVERY_NOMINAL_VENTILATION_VALUE = "pre_defined_remote_boiler_parameters_read_write_flags_ventilation_heat_recovery_nominal_ventilation_value"

# §5.3.5 Class 5, IDs 48/49: upper/lower adjustment bounds for the DHW Setpoint / max CH water Setpoint.
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_UPPER_BOUND = (
    "pre_defined_remote_boiler_parameters_dhwsetp_upper_bound"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHWSETP_LOWER_BOUND = (
    "pre_defined_remote_boiler_parameters_dhwsetp_lower_bound"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_UPPER_BOUND = (
    "pre_defined_remote_boiler_parameters_max_chsetp_upper_bound"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CHSETP_LOWER_BOUND = (
    "pre_defined_remote_boiler_parameters_max_chsetp_lower_bound"
)

# §5.3.5 Class 5, IDs 56/57/87: the remote boiler parameters themselves -- R/W ids, same "_set" number
# takes priority over plain sensor" pattern as Class 4's IDs 27/38/78/79.
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT = (
    "pre_defined_remote_boiler_parameters_dhw_setpoint"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_DHW_SETPOINT_SET = (
    "pre_defined_remote_boiler_parameters_dhw_setpoint_set"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT = (
    "pre_defined_remote_boiler_parameters_max_ch_water_setpoint"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_MAX_CH_WATER_SETPOINT_SET = (
    "pre_defined_remote_boiler_parameters_max_ch_water_setpoint_set"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE = (
    "pre_defined_remote_boiler_parameters_nominal_ventilation_value"
)
CONF_PRE_DEFINED_REMOTE_BOILER_PARAMETERS_NOMINAL_VENTILATION_VALUE_SET = (
    "pre_defined_remote_boiler_parameters_nominal_ventilation_value_set"
)

# §5.3.6 Class 6, IDs 10/88/105 HB: number of TSPs supported, one per family.
CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS = (
    "transparent_boiler_parameters_number_of_tsps"
)
CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_VENTILATION_HEAT_RECOVERY = (
    "transparent_boiler_parameters_number_of_tsps_ventilation_heat_recovery"
)
CONF_TRANSPARENT_BOILER_PARAMETERS_NUMBER_OF_TSPS_SOLAR_STORAGE = (
    "transparent_boiler_parameters_number_of_tsps_solar_storage"
)

# §5.3.6 Class 6, IDs 11/89/106: user-named, user-indexed TSP slots -- one list per family. TSP values
# are opaque and manufacturer-specific, so unlike every other option there's no fixed name/description
# the spec can supply; the user names and indexes whichever slots their boiler documents.
CONF_TRANSPARENT_BOILER_PARAMETERS = "transparent_boiler_parameters"
CONF_TRANSPARENT_BOILER_PARAMETERS_VENTILATION_HEAT_RECOVERY = (
    "transparent_boiler_parameters_ventilation_heat_recovery"
)
CONF_TRANSPARENT_BOILER_PARAMETERS_SOLAR_STORAGE = (
    "transparent_boiler_parameters_solar_storage"
)

# §5.3.7 Class 7, IDs 12/90/107 HB: size of the fault history buffer, one per family.
CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER = "fault_history_data_size_of_fault_buffer"
CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY = (
    "fault_history_data_size_of_fault_buffer_ventilation_heat_recovery"
)
CONF_FAULT_HISTORY_DATA_SIZE_OF_FAULT_BUFFER_SOLAR_STORAGE = (
    "fault_history_data_size_of_fault_buffer_solar_storage"
)

# §5.3.7 Class 7, IDs 13/91/108: user-named, user-indexed fault-history-buffer slots -- one list per
# family. Like Class 6's TSPs, entry values are opaque and manufacturer-specific.
CONF_FAULT_HISTORY_DATA_FAULT_BUFFER = "fault_history_data_fault_buffer"
CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_VENTILATION_HEAT_RECOVERY = (
    "fault_history_data_fault_buffer_ventilation_heat_recovery"
)
CONF_FAULT_HISTORY_DATA_FAULT_BUFFER_SOLAR_STORAGE = (
    "fault_history_data_fault_buffer_solar_storage"
)

# §5.3.8.1 Class 8, ID 7: Cooling control signal (write-only).
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_COOLING_CONTROL_SIGNAL = (
    "control_of_special_applications_cooling_control_signal"
)

# §5.3.8.2 Class 8, ID 14: Maximum relative modulation level setting (write-only).
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_RELATIVE_MODULATION_LEVEL_SETTING = (
    "control_of_special_applications_maximum_relative_modulation_level_setting"
)
# §5.3.8.2 Class 8, ID 15: HB Maximum boiler capacity; LB Minimum modulation level.
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MAXIMUM_BOILER_CAPACITY = (
    "control_of_special_applications_maximum_boiler_capacity"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MINIMUM_MODULATION_LEVEL = (
    "control_of_special_applications_minimum_modulation_level"
)

# §5.3.8.3 Class 8, IDs 9/39: Remote Override Room Setpoint (1 and 2). 0 = no override, 1..30 = value.
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT = (
    "control_of_special_applications_remote_override_room_setpoint"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_2 = (
    "control_of_special_applications_remote_override_room_setpoint_2"
)

# §5.3.8.3 Class 8, ID 99: Remote Override Operating Mode (DHW / Heating HC1 / Heating HC2), plus the
# Manual DHW push2 action bit.
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_DHW = (
    "control_of_special_applications_remote_override_operating_mode_dhw"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC1 = (
    "control_of_special_applications_remote_override_operating_mode_heating_hc1"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_OPERATING_MODE_HEATING_HC2 = (
    "control_of_special_applications_remote_override_operating_mode_heating_hc2"
)
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_MANUAL_DHW_PUSH2 = (
    "control_of_special_applications_manual_dhw_push2"
)

# §5.3.8.3 Class 8, ID 100 LB: Remote Override Room Setpoint function.
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_MANUAL_CHANGE_PRIORITY = "control_of_special_applications_remote_override_room_setpoint_function_manual_change_priority"
CONF_CONTROL_OF_SPECIAL_APPLICATIONS_REMOTE_OVERRIDE_ROOM_SETPOINT_FUNCTION_PROGRAM_CHANGE_PRIORITY = "control_of_special_applications_remote_override_room_setpoint_function_program_change_priority"
