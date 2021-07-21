import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_PIPSOLAR_ID,
)
from .. import PipsolarComponent

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


pipsolar_binary_sensor_ns = cg.esphome_ns.namespace("pipsolarbinarysensor")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(pipsolar_binary_sensor_ns),
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
        cv.Optional(CONF_ADD_SBU_PRIORITY_VERSION): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_CONFIGURATION_STATUS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_SCC_FIRMWARE_VERSION): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_LOAD_STATUS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_CHARGING_STATUS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_SCC_CHARGING_STATUS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_AC_CHARGING_STATUS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_CHARGING_TO_FLOATING_MODE): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_SWITCH_ON): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_DUSTPROOF_INSTALLED): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_SILENCE_BUZZER_OPEN_BUZZER
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_OVERLOAD_BYPASS_FUNCTION): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_LCD_ESCAPE_TO_DEFAULT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_OVERLOAD_RESTART_FUNCTION): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_OVER_TEMPERATURE_RESTART_FUNCTION
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_BACKLIGHT_ON): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_ALARM_ON_WHEN_PRIMARY_SOURCE_INTERRUPT
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_CODE_RECORD): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_POWER_SAVING): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNINGS_PRESENT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULTS_PRESENT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_POWER_LOSS): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_INVERTER_FAULT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_BUS_OVER): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_BUS_UNDER): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_BUS_SOFT_FAIL): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_LINE_FAIL): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_OPVSHORT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_FAULT_INVERTER_VOLTAGE_TOO_LOW
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_FAULT_INVERTER_VOLTAGE_TOO_HIGH
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_OVER_TEMPERATURE): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_FAN_LOCK): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_WARNING_BATTERY_VOLTAGE_HIGH
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_BATTERY_LOW_ALARM): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_WARNING_BATTERY_UNDER_SHUTDOWN
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_BATTERY_DERATING): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_OVER_LOAD): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_EEPROM_FAILED): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_FAULT_INVERTER_OVER_CURRENT
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_FAULT_INVERTER_SOFT_FAILED
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_SELF_TEST_FAILED): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_OP_DC_VOLTAGE_OVER): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_BATTERY_OPEN): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_FAULT_CURRENT_SENSOR_FAILED
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_BATTERY_SHORT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_POWER_LIMIT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_PV_VOLTAGE_HIGH): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_MPPT_OVERLOAD): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNING_MPPT_OVERLOAD): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_WARNING_BATTERY_TOO_LOW_TO_CHARGE
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_DC_DC_OVER_CURRENT): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_FAULT_CODE): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(CONF_WARNUNG_LOW_PV_ENERGY): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_WARNING_HIGH_AC_INPUT_DURING_BUS_SOFT_START
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
        cv.Optional(
            CONF_WARNING_BATTERY_EQUALIZATION
        ): binary_sensor.BINARY_SENSOR_SCHEMA,
    }
)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
    if CONF_ADD_SBU_PRIORITY_VERSION in config:
        conf = config[CONF_ADD_SBU_PRIORITY_VERSION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_add_sbu_priority_version_sensor(sens))
    if CONF_CONFIGURATION_STATUS in config:
        conf = config[CONF_CONFIGURATION_STATUS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_configuration_status_sensor(sens))
    if CONF_SCC_FIRMWARE_VERSION in config:
        conf = config[CONF_SCC_FIRMWARE_VERSION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_scc_firmware_version_sensor(sens))
    if CONF_LOAD_STATUS in config:
        conf = config[CONF_LOAD_STATUS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_load_status_sensor(sens))
    if CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING in config:
        conf = config[CONF_BATTERY_VOLTAGE_TO_STEADY_WHILE_CHARGING]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_battery_voltage_to_steady_while_charging_sensor(sens))
    if CONF_CHARGING_STATUS in config:
        conf = config[CONF_CHARGING_STATUS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_charging_status_sensor(sens))
    if CONF_SCC_CHARGING_STATUS in config:
        conf = config[CONF_SCC_CHARGING_STATUS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_scc_charging_status_sensor(sens))
    if CONF_AC_CHARGING_STATUS in config:
        conf = config[CONF_AC_CHARGING_STATUS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_ac_charging_status_sensor(sens))
    if CONF_CHARGING_TO_FLOATING_MODE in config:
        conf = config[CONF_CHARGING_TO_FLOATING_MODE]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_charging_to_floating_mode_sensor(sens))
    if CONF_SWITCH_ON in config:
        conf = config[CONF_SWITCH_ON]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_switch_on_sensor(sens))
    if CONF_DUSTPROOF_INSTALLED in config:
        conf = config[CONF_DUSTPROOF_INSTALLED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_dustproof_installed_sensor(sens))
    if CONF_SILENCE_BUZZER_OPEN_BUZZER in config:
        conf = config[CONF_SILENCE_BUZZER_OPEN_BUZZER]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_silence_buzzer_open_buzzer_sensor(sens))
    if CONF_OVERLOAD_BYPASS_FUNCTION in config:
        conf = config[CONF_OVERLOAD_BYPASS_FUNCTION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_overload_bypass_function_sensor(sens))
    if CONF_LCD_ESCAPE_TO_DEFAULT in config:
        conf = config[CONF_LCD_ESCAPE_TO_DEFAULT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_lcd_escape_to_default_sensor(sens))
    if CONF_OVERLOAD_RESTART_FUNCTION in config:
        conf = config[CONF_OVERLOAD_RESTART_FUNCTION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_overload_restart_function_sensor(sens))
    if CONF_OVER_TEMPERATURE_RESTART_FUNCTION in config:
        conf = config[CONF_OVER_TEMPERATURE_RESTART_FUNCTION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_over_temperature_restart_function_sensor(sens))
    if CONF_BACKLIGHT_ON in config:
        conf = config[CONF_BACKLIGHT_ON]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_backlight_on_sensor(sens))
    if CONF_ALARM_ON_WHEN_PRIMARY_SOURCE_INTERRUPT in config:
        conf = config[CONF_ALARM_ON_WHEN_PRIMARY_SOURCE_INTERRUPT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_alarm_on_when_primary_source_interrupt_sensor(sens))
    if CONF_FAULT_CODE_RECORD in config:
        conf = config[CONF_FAULT_CODE_RECORD]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_code_record_sensor(sens))
    if CONF_WARNINGS_PRESENT in config:
        conf = config[CONF_WARNINGS_PRESENT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warnings_present_sensor(sens))
    if CONF_FAULTS_PRESENT in config:
        conf = config[CONF_FAULTS_PRESENT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_faults_present_sensor(sens))
    if CONF_WARNING_POWER_LOSS in config:
        conf = config[CONF_WARNING_POWER_LOSS]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_power_loss_sensor(sens))
    if CONF_FAULT_INVERTER_FAULT in config:
        conf = config[CONF_FAULT_INVERTER_FAULT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_inverter_fault_sensor(sens))
    if CONF_FAULT_BUS_OVER in config:
        conf = config[CONF_FAULT_BUS_OVER]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_bus_over_sensor(sens))
    if CONF_FAULT_BUS_UNDER in config:
        conf = config[CONF_FAULT_BUS_UNDER]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_bus_under_sensor(sens))
    if CONF_FAULT_BUS_SOFT_FAIL in config:
        conf = config[CONF_FAULT_BUS_SOFT_FAIL]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_bus_soft_fail_sensor(sens))
    if CONF_WARNING_LINE_FAIL in config:
        conf = config[CONF_WARNING_LINE_FAIL]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_line_fail_sensor(sens))
    if CONF_FAULT_OPVSHORT in config:
        conf = config[CONF_FAULT_OPVSHORT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_opvshort_sensor(sens))
    if CONF_FAULT_INVERTER_VOLTAGE_TOO_LOW in config:
        conf = config[CONF_FAULT_INVERTER_VOLTAGE_TOO_LOW]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_inverter_voltage_too_low_sensor(sens))
    if CONF_FAULT_INVERTER_VOLTAGE_TOO_HIGH in config:
        conf = config[CONF_FAULT_INVERTER_VOLTAGE_TOO_HIGH]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_inverter_voltage_too_high_sensor(sens))
    if CONF_WARNING_OVER_TEMPERATURE in config:
        conf = config[CONF_WARNING_OVER_TEMPERATURE]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_over_temperature_sensor(sens))
    if CONF_WARNING_FAN_LOCK in config:
        conf = config[CONF_WARNING_FAN_LOCK]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_fan_lock_sensor(sens))
    if CONF_WARNING_BATTERY_VOLTAGE_HIGH in config:
        conf = config[CONF_WARNING_BATTERY_VOLTAGE_HIGH]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_voltage_high_sensor(sens))
    if CONF_WARNING_BATTERY_LOW_ALARM in config:
        conf = config[CONF_WARNING_BATTERY_LOW_ALARM]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_low_alarm_sensor(sens))
    if CONF_WARNING_BATTERY_UNDER_SHUTDOWN in config:
        conf = config[CONF_WARNING_BATTERY_UNDER_SHUTDOWN]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_under_shutdown_sensor(sens))
    if CONF_WARNING_BATTERY_DERATING in config:
        conf = config[CONF_WARNING_BATTERY_DERATING]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_derating_sensor(sens))
    if CONF_WARNING_OVER_LOAD in config:
        conf = config[CONF_WARNING_OVER_LOAD]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_over_load_sensor(sens))
    if CONF_WARNING_EEPROM_FAILED in config:
        conf = config[CONF_WARNING_EEPROM_FAILED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_eeprom_failed_sensor(sens))
    if CONF_FAULT_INVERTER_OVER_CURRENT in config:
        conf = config[CONF_FAULT_INVERTER_OVER_CURRENT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_inverter_over_current_sensor(sens))
    if CONF_FAULT_INVERTER_SOFT_FAILED in config:
        conf = config[CONF_FAULT_INVERTER_SOFT_FAILED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_inverter_soft_failed_sensor(sens))
    if CONF_FAULT_SELF_TEST_FAILED in config:
        conf = config[CONF_FAULT_SELF_TEST_FAILED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_self_test_failed_sensor(sens))
    if CONF_FAULT_OP_DC_VOLTAGE_OVER in config:
        conf = config[CONF_FAULT_OP_DC_VOLTAGE_OVER]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_op_dc_voltage_over_sensor(sens))
    if CONF_FAULT_BATTERY_OPEN in config:
        conf = config[CONF_FAULT_BATTERY_OPEN]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_battery_open_sensor(sens))
    if CONF_FAULT_CURRENT_SENSOR_FAILED in config:
        conf = config[CONF_FAULT_CURRENT_SENSOR_FAILED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_current_sensor_failed_sensor(sens))
    if CONF_FAULT_BATTERY_SHORT in config:
        conf = config[CONF_FAULT_BATTERY_SHORT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_battery_short_sensor(sens))
    if CONF_WARNING_POWER_LIMIT in config:
        conf = config[CONF_WARNING_POWER_LIMIT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_power_limit_sensor(sens))
    if CONF_WARNING_PV_VOLTAGE_HIGH in config:
        conf = config[CONF_WARNING_PV_VOLTAGE_HIGH]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_pv_voltage_high_sensor(sens))
    if CONF_FAULT_MPPT_OVERLOAD in config:
        conf = config[CONF_FAULT_MPPT_OVERLOAD]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_mppt_overload_sensor(sens))
    if CONF_WARNING_MPPT_OVERLOAD in config:
        conf = config[CONF_WARNING_MPPT_OVERLOAD]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_mppt_overload_sensor(sens))
    if CONF_WARNING_BATTERY_TOO_LOW_TO_CHARGE in config:
        conf = config[CONF_WARNING_BATTERY_TOO_LOW_TO_CHARGE]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_too_low_to_charge_sensor(sens))
    if CONF_FAULT_DC_DC_OVER_CURRENT in config:
        conf = config[CONF_FAULT_DC_DC_OVER_CURRENT]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_dc_dc_over_current_sensor(sens))
    if CONF_FAULT_CODE in config:
        conf = config[CONF_FAULT_CODE]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_fault_code_sensor(sens))
    if CONF_WARNUNG_LOW_PV_ENERGY in config:
        conf = config[CONF_WARNUNG_LOW_PV_ENERGY]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warnung_low_pv_energy_sensor(sens))
    if CONF_WARNING_HIGH_AC_INPUT_DURING_BUS_SOFT_START in config:
        conf = config[CONF_WARNING_HIGH_AC_INPUT_DURING_BUS_SOFT_START]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_high_ac_input_during_bus_soft_start_sensor(sens))
    if CONF_WARNING_BATTERY_EQUALIZATION in config:
        conf = config[CONF_WARNING_BATTERY_EQUALIZATION]
        sens = cg.new_Pvariable(conf[CONF_ID])
        yield binary_sensor.register_binary_sensor(sens, conf)
        cg.add(paren.set_warning_battery_equalization_sensor(sens))
