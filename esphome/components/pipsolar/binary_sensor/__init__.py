import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
)
from .. import PIPSOLAR_COMPONENT_SCHEMA, CONF_PIPSOLAR_ID

DEPENDENCIES = ["uart"]

CONF_ADD_SBU_PRIORITY_VERSION = "add_sbu_priority_version"
CONF_CONFIGURATION_STATUS = "configuration_status"
CONF_SCC_FIRMWARE_VERSION = "scc_firmware_version"
CONF_LOAD_STATUS = "load_status"
CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING = (
    "battery_voltage_to_steady_while_charging"
)
CONF_CHARGING_STATUS = "charging_status"
CONF_SCC_CHARGING_STATUS = "scc_charging_status"
CONF_AC_CHARGING_STATUS = "ac_charging_status"
CONF_CHARGING_TO_FLOATING_MODE = "charging_to_floating_mode"
CONF_SWITCH_ON = "switch_on"
CONF_DUSTPROOF_INSTALLED = "dustproof_installed"
CONF_SILENCE_BUZZER_OPEN_BUZZER = "silence_buzzer_open_buzzer"
CONF_OVERLOAD_BYPASS_FUNCTION = "overload_bypass_function"
CONF_LCD_ESCAPE_TO_DEFAULT = "lcd_escape_to_default"
CONF_OVERLOAD_RESTART_FUNCTION = "overload_restart_function"
CONF_OVER_TEMPERATURE_RESTART_FUNCTION = "over_temperature_restart_function"
CONF_BACKLIGHT_ON = "backlight_on"
CONF_ALARM_ON_WHEN_PRIMARY_SOURCE_INTERRUPT = "alarm_on_when_primary_source_interrupt"
CONF_FAULT_CODE_RECORD = "fault_code_record"
CONF_POWER_SAVING = "power_saving"

CONF_WARNINGS_PRESENT = "warnings_present"
CONF_FAULTS_PRESENT = "faults_present"
CONF_WARNING_POWER_LOSS = "warning_power_loss"
CONF_FAULT_INVERTER_FAULT = "fault_inverter_fault"
CONF_FAULT_BUS_OVER = "fault_bus_over"
CONF_FAULT_BUS_UNDER = "fault_bus_under"
CONF_FAULT_BUS_SOFT_FAIL = "fault_bus_soft_fail"
CONF_WARNING_LINE_FAIL = "warning_line_fail"
CONF_FAULT_OPVSHORT = "fault_opvshort"
CONF_FAULT_INVERTER_VOLTAGE_TOO_LOW = "fault_inverter_voltage_too_low"
CONF_FAULT_INVERTER_VOLTAGE_TOO_HIGH = "fault_inverter_voltage_too_high"
CONF_WARNING_OVER_TEMPERATURE = "warning_over_temperature"
CONF_WARNING_FAN_LOCK = "warning_fan_lock"
CONF_WARNING_BATTERY_VOLTAGE_HIGH = "warning_battery_voltage_high"
CONF_WARNING_BATTERY_LOW_ALARM = "warning_battery_low_alarm"
CONF_WARNING_BATTERY_UNDER_SHUTDOWN = "warning_battery_under_shutdown"
CONF_WARNING_BATTERY_DERATING = "warning_battery_derating"
CONF_WARNING_OVER_LOAD = "warning_over_load"
CONF_WARNING_EEPROM_FAILED = "warning_eeprom_failed"
CONF_FAULT_INVERTER_OVER_CURRENT = "fault_inverter_over_current"
CONF_FAULT_INVERTER_SOFT_FAILED = "fault_inverter_soft_failed"
CONF_FAULT_SELF_TEST_FAILED = "fault_self_test_failed"
CONF_FAULT_OP_DC_VOLTAGE_OVER = "fault_op_dc_voltage_over"
CONF_FAULT_BATTERY_OPEN = "fault_battery_open"
CONF_FAULT_CURRENT_SENSOR_FAILED = "fault_current_sensor_failed"
CONF_FAULT_BATTERY_SHORT = "fault_battery_short"
CONF_WARNING_POWER_LIMIT = "warning_power_limit"
CONF_WARNING_PV_VOLTAGE_HIGH = "warning_pv_voltage_high"
CONF_FAULT_MPPT_OVERLOAD = "fault_mppt_overload"
CONF_WARNING_MPPT_OVERLOAD = "warning_mppt_overload"
CONF_WARNING_BATTERY_TOO_LOW_TO_CHARGE = "warning_battery_too_low_to_charge"
CONF_FAULT_DC_DC_OVER_CURRENT = "fault_dc_dc_over_current"
CONF_FAULT_CODE = "fault_code"
CONF_WARNUNG_LOW_PV_ENERGY = "warnung_low_pv_energy"
CONF_WARNING_HIGH_AC_INPUT_DURING_BUS_SOFT_START = (
    "warning_high_ac_input_during_bus_soft_start"
)
CONF_WARNING_BATTERY_EQUALIZATION = "warning_battery_equalization"

TYPES = [
    CONF_ADD_SBU_PRIORITY_VERSION,
    CONF_CONFIGURATION_STATUS,
    CONF_SCC_FIRMWARE_VERSION,
    CONF_LOAD_STATUS,
    CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING,
    CONF_CHARGING_STATUS,
    CONF_SCC_CHARGING_STATUS,
    CONF_AC_CHARGING_STATUS,
    CONF_CHARGING_TO_FLOATING_MODE,
    CONF_SWITCH_ON,
    CONF_DUSTPROOF_INSTALLED,
    CONF_SILENCE_BUZZER_OPEN_BUZZER,
    CONF_OVERLOAD_BYPASS_FUNCTION,
    CONF_LCD_ESCAPE_TO_DEFAULT,
    CONF_OVERLOAD_RESTART_FUNCTION,
    CONF_OVER_TEMPERATURE_RESTART_FUNCTION,
    CONF_BACKLIGHT_ON,
    CONF_ALARM_ON_WHEN_PRIMARY_SOURCE_INTERRUPT,
    CONF_FAULT_CODE_RECORD,
    CONF_POWER_SAVING,
    CONF_WARNINGS_PRESENT,
    CONF_FAULTS_PRESENT,
    CONF_WARNING_POWER_LOSS,
    CONF_FAULT_INVERTER_FAULT,
    CONF_FAULT_BUS_OVER,
    CONF_FAULT_BUS_UNDER,
    CONF_FAULT_BUS_SOFT_FAIL,
    CONF_WARNING_LINE_FAIL,
    CONF_FAULT_OPVSHORT,
    CONF_FAULT_INVERTER_VOLTAGE_TOO_LOW,
    CONF_FAULT_INVERTER_VOLTAGE_TOO_HIGH,
    CONF_WARNING_OVER_TEMPERATURE,
    CONF_WARNING_FAN_LOCK,
    CONF_WARNING_BATTERY_VOLTAGE_HIGH,
    CONF_WARNING_BATTERY_LOW_ALARM,
    CONF_WARNING_BATTERY_UNDER_SHUTDOWN,
    CONF_WARNING_BATTERY_DERATING,
    CONF_WARNING_OVER_LOAD,
    CONF_WARNING_EEPROM_FAILED,
    CONF_FAULT_INVERTER_OVER_CURRENT,
    CONF_FAULT_INVERTER_SOFT_FAILED,
    CONF_FAULT_SELF_TEST_FAILED,
    CONF_FAULT_OP_DC_VOLTAGE_OVER,
    CONF_FAULT_BATTERY_OPEN,
    CONF_FAULT_CURRENT_SENSOR_FAILED,
    CONF_FAULT_BATTERY_SHORT,
    CONF_WARNING_POWER_LIMIT,
    CONF_WARNING_PV_VOLTAGE_HIGH,
    CONF_FAULT_MPPT_OVERLOAD,
    CONF_WARNING_MPPT_OVERLOAD,
    CONF_WARNING_BATTERY_TOO_LOW_TO_CHARGE,
    CONF_FAULT_DC_DC_OVER_CURRENT,
    CONF_FAULT_CODE,
    CONF_WARNUNG_LOW_PV_ENERGY,
    CONF_WARNING_HIGH_AC_INPUT_DURING_BUS_SOFT_START,
    CONF_WARNING_BATTERY_EQUALIZATION,
]

CONFIG_SCHEMA = PIPSOLAR_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): binary_sensor.BINARY_SENSOR_SCHEMA for type in TYPES}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PIPSOLAR_ID])
    for type in TYPES:
        if type in config:
            conf = config[type]
            sens = cg.new_Pvariable(conf[CONF_ID])
            await binary_sensor.register_binary_sensor(sens, conf)
            cg.add(getattr(paren, f"set_{type}")(sens))
