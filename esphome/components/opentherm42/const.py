# Config keys shared by opentherm42's platform files (switch/number/binary_sensor/sensor). Named
# after the OpenTherm Protocol Specification v4.2 section + field hierarchy, with "HB"/"LB" stripped
# and the spec's term for the boiler side of the conversation replaced by "boiler" (see CLAUDE.md's
# C++ enumerator-naming rule for why that term is avoided project-wide).

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
