import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, modbus, binary_sensor, mqtt
from esphome.core import coroutine
from esphome.util import Registry

from esphome.const import CONF_MQTT_ID, CONF_ID, ICON_CURRENT_AC, \
    UNIT_VOLT, ICON_FLASH, UNIT_AMPERE, UNIT_WATT, UNIT_EMPTY, CONF_BATTERY_VOLTAGE, \
    ICON_POWER, UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, ICON_PERCENT, ICON_BATTERY, \
    ICON_MOLECULE_CO2, UNIT_MINUTE, UNIT_SECOND, ICON_EMPTY, CONF_ADDRESS, CONF_OFFSET

from .const import CONF_ARRAY_RATED_VOLTAGE, CONF_ARRAY_RATED_CURRENT, CONF_ARRAY_RATED_POWER, \
    CONF_BATTERY_RATED_VOLTAGE, CONF_BATTERY_RATED_CURRENT, CONF_BATTERY_RATED_POWER, \
    CONF_CHARGING_MODE, CONF_RATED_CURRENT_OF_LOAD, CONF_PV_INPUT_VOLTAGE,  \
    CONF_PV_INPUT_CURRENT, CONF_PV_POWER, CONF_BATTERY_POWER, CONF_LOAD_VOLTAGE, \
    CONF_LOAD_CURRENT, CONF_LOAD_POWER, CONF_BATTERY_TEMPERATURE, CONF_DEVICE_TEMPERATURE, \
    CONF_BATTERY_SOC, CONF_REMOTE_BATTERY_TEMPERATURE, CONF_BATTERY_STATUS, CONF_CHARGING_STATUS, \
    CONF_DISCHARGING_STATUS, CONF_MAX_PV_VOLTAGE_TODAY, CONF_MIN_PV_VOLTAGE_TODAY, \
    CONF_MAX_BATTERY_VOLTAGE_TODAY, CONF_MIN_BATTERY_TODAY, CONF_CONSUMED_ENERGY_TODAY, \
    CONF_CONSUMED_ENERGY_MONTH, CONF_CONSUMED_ENERGY_YEAR, CONF_CONSUMED_ENERGY_TOTAL, \
    CONF_GENERATED_ENERGY_TODAY, CONF_GENERATED_ENERGY_MONTH, CONF_GENERATED_ENERGY_YEAR, \
    CONF_GENERATED_ENERGY_TOTAL, CONF_BATTERY_CURRENT, CONF_BATTERY_TYPE, CONF_BATTERY_CAPACITY, \
    CONF_CO2_REDUCTION, CONF_HIGH_VOLT_DISCONNECT, CONF_TEMPERATURE_COMPENSATION_COEFFICIENT, \
    CONF_CHARGING_LIMIT_VOLTAGE, CONF_OVER_VOLTAGE_RECONNECT, CONF_EQUALIZATION_VOLTAGE, \
    CONF_BOOST_VOLTAGE, CONF_FLOAT_VOLTAGE, CONF_BOOST_RECONNECT_VOLTAGE, \
    CONF_LOW_VOLTAGE_RECONNECT, CONF_UNDER_VOLTAGE_RECOVER, CONF_UNDER_VOLTAGE_WARNING, \
    CONF_LOW_VOLTAGE_DISCONNECT, CONF_SYNC_RTC, CONF_DISCHARGING_LIMIT_VOLTAGE, \
    CONF_BATTERY_RESISTENCE_ERROR, CONF_EQUALIZATION_CHARGING_CYCLE, \
    CONF_BATTERY_TEMPERATURE_WARNING_UPPER_LIMIT, CONF_BATTERY_TEMPERATURE_WARNING_LOWER_LIMIT, \
    CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT, \
    CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT_RECOVER, \
    CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT, \
    CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT_RECOVER, \
    CONF_LINE_IMPEDANCE, CONF_DAY_TIME_THRESHOLD_VOLTAGE, CONF_LIGHT_SIGNAL_STARTUP_DELAY_TIME, \
    CONF_NIGHT_TIMETHRESHOLD_VOLTAGE, CONF_LIGHT_SIGNAL_TURN_OFF_DELAY_TIME,  \
    CONF_DEVICE_OVERTEMPERATURE, CONF_IS_NIGHT, CONF_MANUAL_CONTROL_LOAD, \
    CONF_ENABLE_LOAD_TEST_MODE, CONF_FORCE_LOAD_ON, CONF_LOAD_CONTROL_MODE, \
    CONF_WORKING_TIME_LENGTH_1, CONF_WORKING_TIME_LENGTH_2, CONF_TURN_ON_TIMING_1_SEC, \
    CONF_TURN_ON_TIMING_1_MIN, CONF_TURN_ON_TIMING_1_HOUR, \
    CONF_TURN_ON_TIMING_2_SEC, CONF_TURN_ON_TIMING_2_MIN, CONF_TURN_ON_TIMING_2_HOUR, \
    CONF_TURN_OFF_TIMING_1_SEC, CONF_TURN_OFF_TIMING_1_MIN, CONF_TURN_OFF_TIMING_1_HOUR, \
    CONF_TURN_OFF_TIMING_2_SEC, CONF_TURN_OFF_TIMING_2_MIN, CONF_TURN_OFF_TIMING_2_HOUR, \
    CONF_BACKLIGHT_TIME, CONF_LENGTH_OF_NIGHT, CONF_MAIN_POWER_SUPPLY, \
    CONF_BATTERY_RATED_VOLTAGE_CODE, CONF_DEFAULT_LOAD_MODE, CONF_EQUALIZE_DURATION, \
    CONF_BOOST_DURATION, CONF_DISCHARGE_PERCENTAGE, CONF_CHARGING_PERCENTAGE, \
    CONF_BATTERY_MANAGEMENT_MODE, CONF_MODBUSDEVICE_ADDRESS, CONF_VALUE_TYPE, \
    CONF_SCALE_FACTOR, CONF_MODBUS_FUNCTIONCODE, CONF_BITMASK, \
    UNIT_AMPERE_HOURS, UNIT_KG, UNIT_KWATT_HOURS, UNIT_MILLIOHM, UNIT_HOURS


AUTO_LOAD = ['modbus', 'binary_sensor', 'status', 'mqtt']

epsolar_ns = cg.esphome_ns.namespace('epsolar')
EPSOLAR = epsolar_ns.class_('EPSOLAR', cg.PollingComponent, modbus.ModbusDevice)

ModbusFunctionCode_ns = cg.esphome_ns.namespace('epsolar::ModbusFunctionCode')
ModbusFunctionCode = ModbusFunctionCode_ns.enum('ModbusFunctionCode')
MODBUS_FUNCTION_CODE = {
    'read_coils': ModbusFunctionCode.READ_COILS,
    'read_discrete_inputs': ModbusFunctionCode.READ_DISCRETE_INPUTS,
    'read_holding_registers': ModbusFunctionCode.READ_HOLDING_REGISTERS,
    'read_input_registers': ModbusFunctionCode.READ_INPUT_REGISTERS,
    'write_single_coil': ModbusFunctionCode.WRITE_SINGLE_COIL,
    'write_single_register': ModbusFunctionCode.WRITE_SINGLE_REGISTER,
    'write_multiple_registers': ModbusFunctionCode.WRITE_MULTIPLE_REGISTERS
}


SensorValueType_ns = cg.esphome_ns.namespace('epsolar::SensorValueType')
SensorValueType = SensorValueType_ns.enum('SensorValueType')
SENSOR_VALUE_TYPE = {
    'RAW': SensorValueType.RAW,
    'U_SINGLE': SensorValueType.U_SINGLE,
    'U_DOUBLE': SensorValueType.U_DOUBLE,
    'S_SINGLE': SensorValueType.S_SINGLE,
    'S_DOUBLE': SensorValueType.S_DOUBLE
}

MODBUS_REGISTRY = Registry()
validate_modbus_range = cv.validate_registry('sensors', MODBUS_REGISTRY)

sensor_entry = sensor.SENSOR_SCHEMA.extend({
    cv.Optional(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
    cv.Optional(CONF_ADDRESS): cv.int_,
    cv.Optional(CONF_OFFSET): cv.int_,
    cv.Optional(CONF_BITMASK, default=0xFFFF): cv.int_,
    cv.Optional(CONF_VALUE_TYPE): cv.enum(SENSOR_VALUE_TYPE),
    cv.Optional(CONF_SCALE_FACTOR): cv.float_,
})

binary_sensor_entry = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.Optional(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
    cv.Optional(CONF_ADDRESS): cv.int_,
    cv.Optional(CONF_OFFSET): cv.int_,
    cv.Optional(CONF_BITMASK, default=0x1): cv.int_,
})


def modbus_sensor_schema(
        modbus_functioncode_, register_address_, register_offset_,
        bitmask_, value_type_, scale_factor_,
        unit_of_measurement_, icon_, accuracy_decimals_):
    return sensor.sensor_schema(unit_of_measurement_, icon_, accuracy_decimals_, ).extend({
        cv.Optional(CONF_MODBUS_FUNCTIONCODE, default=modbus_functioncode_):
        cv.enum(MODBUS_FUNCTION_CODE),
        cv.Optional(CONF_ADDRESS, default=register_address_): cv.int_,
        cv.Optional(CONF_OFFSET, default=register_offset_): cv.int_,
        cv.Optional(CONF_BITMASK, default=bitmask_): cv.int_,
        cv.Optional(CONF_VALUE_TYPE, default=value_type_): cv.enum(SENSOR_VALUE_TYPE),
        cv.Optional(CONF_SCALE_FACTOR, default=scale_factor_): cv.float_,
    })


def modbus_binarysensor_schema(
        modbus_functioncode_, register_address_, register_offset_, bitmask_=1):
    return binary_sensor.BINARY_SENSOR_SCHEMA.extend({
        cv.Optional(CONF_MODBUS_FUNCTIONCODE, default=modbus_functioncode_):
        cv.enum(MODBUS_FUNCTION_CODE),
        cv.Optional(CONF_ADDRESS, default=register_address_): cv.int_,
        cv.Optional(CONF_OFFSET, default=register_offset_): cv.int_,
        cv.Optional(CONF_BITMASK, default=bitmask_): cv.int_,
    })


MODBUS_CONFIG_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.Optional(CONF_MODBUSDEVICE_ADDRESS, default=0x1): cv.hex_uint8_t,
    cv.Optional('sensors'): cv.All(cv.ensure_list(sensor_entry), cv.Length(min=0)),
    cv.Optional('binary_sensors'): cv.All(cv.ensure_list(binary_sensor_entry), cv.Length(min=0)),
}).extend(cv.polling_component_schema('60s')).extend(modbus.modbus_device_schema(0x01))


def modbus_component_schema(device_address=0x1):
    return cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
        cv.Optional(CONF_MODBUSDEVICE_ADDRESS, default=0x1): cv.hex_uint8_t,
        cv.Optional('sensors'): cv.All(cv.ensure_list(sensor_entry), cv.Length(min=0)),
        cv.Optional('binary_sensors'):
            cv.All(cv.ensure_list(binary_sensor_entry), cv.Length(min=0)),
    }).extend(cv.polling_component_schema('60s')).extend(
        modbus.modbus_device_schema(device_address))


ALLBITS = 0xFFFF

CONFIG_SCHEMA = MODBUS_CONFIG_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(EPSOLAR),
    cv.Optional(CONF_ARRAY_RATED_VOLTAGE):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x0, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_ARRAY_RATED_CURRENT):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x1, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_AMPERE, ICON_BATTERY, 1),
    cv.Optional(CONF_ARRAY_RATED_POWER):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x2, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_WATT, ICON_BATTERY, 1),
    cv.Optional(CONF_BATTERY_RATED_VOLTAGE):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x4, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_BATTERY_RATED_CURRENT):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x5, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_AMPERE, ICON_BATTERY, 1),
    cv.Optional(CONF_BATTERY_RATED_POWER):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x6, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_WATT, ICON_BATTERY, 1),
    cv.Optional(CONF_CHARGING_MODE):
        modbus_sensor_schema('read_input_registers', 0x3000, 0x8, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_BATTERY, 0),
    cv.Optional(CONF_RATED_CURRENT_OF_LOAD):
        modbus_sensor_schema('read_input_registers', 0x300E, 0x0, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_AMPERE, ICON_BATTERY, 1),
    cv.Optional(CONF_PV_INPUT_VOLTAGE):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x0, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_PV_INPUT_CURRENT):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x1, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_AMPERE, ICON_CURRENT_AC, 2),
    cv.Optional(CONF_PV_POWER):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x2, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_WATT, ICON_POWER, 1),
    cv.Optional(CONF_BATTERY_POWER):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x6, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_WATT, ICON_POWER, 1),
    cv.Optional(CONF_LOAD_VOLTAGE):
        modbus_sensor_schema('read_input_registers', 0x3100, 0xC, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_LOAD_CURRENT):
        modbus_sensor_schema('read_input_registers', 0x3100, 0xD, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_AMPERE, ICON_CURRENT_AC, 1),
    cv.Optional(CONF_LOAD_POWER):
        modbus_sensor_schema('read_input_registers', 0x3100, 0xE, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_WATT, ICON_POWER, 1),
    cv.Optional(CONF_BATTERY_TEMPERATURE):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x10, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_DEVICE_TEMPERATURE):
        modbus_sensor_schema('read_input_registers', 0x3100, 0x11, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_BATTERY_SOC):
        modbus_sensor_schema('read_input_registers', 0x311A, 0x0, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_PERCENT, ICON_PERCENT, 0),
    cv.Optional(CONF_REMOTE_BATTERY_TEMPERATURE):
        modbus_sensor_schema('read_input_registers', 0x311A, 0x1, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_BATTERY_STATUS):
        modbus_sensor_schema('read_input_registers', 0x3200, 0x0, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_BATTERY, 0),
    cv.Optional(CONF_BATTERY_RESISTENCE_ERROR):
        modbus_binarysensor_schema('read_input_registers', 0x3200, 0x0, 256),
    cv.Optional(CONF_CHARGING_STATUS):
        modbus_sensor_schema('read_input_registers', 0x3200, 0x1, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_POWER, 0),
    cv.Optional(CONF_DISCHARGING_STATUS):
        modbus_sensor_schema('read_input_registers', 0x3200, 0x2, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_POWER, 0),
    cv.Optional(CONF_MAX_PV_VOLTAGE_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x0, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_MIN_PV_VOLTAGE_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x1, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_MAX_BATTERY_VOLTAGE_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x2, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_MIN_BATTERY_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x3, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CONSUMED_ENERGY_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x4, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_CONSUMED_ENERGY_MONTH):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x6, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_CONSUMED_ENERGY_YEAR):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x8, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_CONSUMED_ENERGY_TOTAL):
        modbus_sensor_schema('read_input_registers', 0x3300, 0xA, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_GENERATED_ENERGY_TODAY):
        modbus_sensor_schema('read_input_registers', 0x3300, 0xC, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_GENERATED_ENERGY_MONTH):
        modbus_sensor_schema('read_input_registers', 0x3300, 0xE, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_GENERATED_ENERGY_YEAR):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x10, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_GENERATED_ENERGY_TOTAL):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x12, ALLBITS,
                             'U_DOUBLE', 0.01, UNIT_KWATT_HOURS, ICON_POWER, 1),
    cv.Optional(CONF_CO2_REDUCTION):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x14, ALLBITS,
                             'U_SINGLE', 10.0, UNIT_KG, ICON_MOLECULE_CO2, 0),
    cv.Optional(CONF_BATTERY_VOLTAGE):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x1A, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_BATTERY_CURRENT):
        modbus_sensor_schema('read_input_registers', 0x3300, 0x1B, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_AMPERE, ICON_CURRENT_AC, 1),
    cv.Optional(CONF_BATTERY_TYPE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x0, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_BATTERY, 0),
    cv.Optional(CONF_BATTERY_CAPACITY):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x1, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_AMPERE_HOURS, ICON_BATTERY, 0),
    cv.Optional(CONF_TEMPERATURE_COMPENSATION_COEFFICIENT):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x2, ALLBITS,
                             'S_SINGLE', 0.0, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_HIGH_VOLT_DISCONNECT):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x3, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_CHARGING_LIMIT_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x4, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_OVER_VOLTAGE_RECONNECT):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x5, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_EQUALIZATION_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x6, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_BOOST_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x7, ALLBITS,
                             'U_SINGLE', 0.0, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_FLOAT_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x8, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_BOOST_RECONNECT_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0x9, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_LOW_VOLTAGE_RECONNECT):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0xA, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_UNDER_VOLTAGE_RECOVER):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0xB, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_UNDER_VOLTAGE_WARNING):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0xC, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_LOW_VOLTAGE_DISCONNECT):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0xD, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_DISCHARGING_LIMIT_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9000, 0xE, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_EQUALIZATION_CHARGING_CYCLE):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x03, ALLBITS,
                             'U_SINGLE', 1, UNIT_MINUTE, ICON_THERMOMETER, 0),
    cv.Optional(CONF_BATTERY_TEMPERATURE_WARNING_UPPER_LIMIT):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x04, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_BATTERY_TEMPERATURE_WARNING_LOWER_LIMIT):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x05, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x06, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT_RECOVER):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x07, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x08, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT_RECOVER):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x09, ALLBITS,
                             'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_LINE_IMPEDANCE):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0xA, ALLBITS,
                             'S_SINGLE', 1.0, UNIT_MILLIOHM, ICON_EMPTY, 1),
    cv.Optional(CONF_DAY_TIME_THRESHOLD_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x0B, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_BATTERY, 1),
    cv.Optional(CONF_LIGHT_SIGNAL_STARTUP_DELAY_TIME):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x0C, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_MINUTE, ICON_EMPTY, 1),
    cv.Optional(CONF_NIGHT_TIMETHRESHOLD_VOLTAGE):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x0D, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_VOLT, ICON_EMPTY, 1),
    cv.Optional(CONF_LIGHT_SIGNAL_TURN_OFF_DELAY_TIME):
        modbus_sensor_schema('read_holding_registers', 0x9013, 0x0E, ALLBITS,
                             'U_SINGLE', 0.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_LOAD_CONTROL_MODE):
        modbus_sensor_schema('read_holding_registers', 0x903D, 0x00, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_WORKING_TIME_LENGTH_1):
        modbus_sensor_schema('read_holding_registers', 0x903D, 0x01, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_WORKING_TIME_LENGTH_2):
        modbus_sensor_schema('read_holding_registers', 0x903D, 0x02, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_1_SEC):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x00, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_1_MIN):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x01, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_1_HOUR):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x02, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_HOURS, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_1_SEC):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x03, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_1_MIN):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x04, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_1_HOUR):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x05, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_HOURS, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_2_SEC):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x06, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_2_MIN):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x07, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_ON_TIMING_2_HOUR):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x08, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_HOURS, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_2_SEC):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x09, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_1_MIN):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x0A, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_TURN_OFF_TIMING_2_HOUR):
        modbus_sensor_schema('read_holding_registers', 0x9042, 0x0B, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_HOURS, ICON_EMPTY, 0),
    cv.Optional(CONF_BACKLIGHT_TIME):
        modbus_sensor_schema('read_holding_registers', 0x9063, 0x00, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_LENGTH_OF_NIGHT):
        modbus_sensor_schema('read_holding_registers', 0x9063, 0x02, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_SECOND, ICON_EMPTY, 0),
    cv.Optional(CONF_MAIN_POWER_SUPPLY):
        modbus_sensor_schema('read_holding_registers', 0x9063, 0x03, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_EMPTY, 0),
    cv.Optional(CONF_BATTERY_RATED_VOLTAGE_CODE):
        modbus_sensor_schema('read_holding_registers', 0x9063, 0x04, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_EMPTY, 0),
    cv.Optional(CONF_DEFAULT_LOAD_MODE):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x00, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_EMPTY, 0),
    cv.Optional(CONF_EQUALIZE_DURATION):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x01, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_BOOST_DURATION):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x02, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_MINUTE, ICON_EMPTY, 0),
    cv.Optional(CONF_DISCHARGE_PERCENTAGE):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x03, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_PERCENT, ICON_EMPTY, 0),
    cv.Optional(CONF_CHARGING_PERCENTAGE):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x04, ALLBITS,
                             'U_SINGLE', 0.01, UNIT_PERCENT, ICON_EMPTY, 0),
    cv.Optional(CONF_BATTERY_MANAGEMENT_MODE):
        modbus_sensor_schema('read_holding_registers', 0x906A, 0x05, ALLBITS,
                             'U_SINGLE', 1.0, UNIT_EMPTY, ICON_EMPTY, 0),
    cv.Optional(CONF_DEVICE_OVERTEMPERATURE):
        modbus_binarysensor_schema('read_discrete_inputs', 0x2000, 0x0, 0x1),
    cv.Optional(CONF_IS_NIGHT):
        modbus_binarysensor_schema('read_discrete_inputs', 0x200C, 0x0, 0x1),
    cv.Optional(CONF_MANUAL_CONTROL_LOAD):
        modbus_binarysensor_schema('read_coils', 0x2, 0x0, 0b0001),
    cv.Optional(CONF_ENABLE_LOAD_TEST_MODE):
        modbus_binarysensor_schema('read_coils', 0x2, 0x0, 0b0100),
    cv.Optional(CONF_FORCE_LOAD_ON):
        modbus_binarysensor_schema('read_coils', 0x2, 0x0, 0b1000),
    cv.Optional(CONF_SYNC_RTC, default=False): cv.boolean,
}).extend(cv.polling_component_schema('60s')).extend(modbus.modbus_device_schema(0x01))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield modbus.register_modbus_device(var, config)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        yield mqtt.register_mqtt_component(mqtt_, config)
# // Real-time Datum (Read Only)
    if CONF_ARRAY_RATED_VOLTAGE in config:
        conf = config[CONF_ARRAY_RATED_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_ARRAY_RATED_CURRENT in config:
        conf = config[CONF_ARRAY_RATED_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_ARRAY_RATED_POWER in config:
        conf = config[CONF_ARRAY_RATED_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_RATED_VOLTAGE in config:
        conf = config[CONF_BATTERY_RATED_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_RATED_CURRENT in config:
        conf = config[CONF_BATTERY_RATED_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_RATED_POWER in config:
        conf = config[CONF_BATTERY_RATED_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CHARGING_MODE in config:
        conf = config[CONF_CHARGING_MODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_RATED_CURRENT_OF_LOAD in config:
        conf = config[CONF_RATED_CURRENT_OF_LOAD]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_PV_INPUT_VOLTAGE in config:
        conf = config[CONF_PV_INPUT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_PV_INPUT_CURRENT in config:
        conf = config[CONF_PV_INPUT_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_PV_POWER in config:
        conf = config[CONF_PV_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_POWER in config:
        conf = config[CONF_BATTERY_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOAD_VOLTAGE in config:
        conf = config[CONF_LOAD_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOAD_CURRENT in config:
        conf = config[CONF_LOAD_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOAD_POWER in config:
        conf = config[CONF_LOAD_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_TEMPERATURE in config:
        conf = config[CONF_BATTERY_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DEVICE_TEMPERATURE in config:
        conf = config[CONF_DEVICE_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_SOC in config:
        conf = config[CONF_BATTERY_SOC]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_REMOTE_BATTERY_TEMPERATURE in config:
        conf = config[CONF_REMOTE_BATTERY_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
# // Real-time Status (Read Only)
    if CONF_BATTERY_STATUS in config:
        conf = config[CONF_BATTERY_STATUS]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_RESISTENCE_ERROR in config:
        conf = config[CONF_BATTERY_RESISTENCE_ERROR]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                                    conf[CONF_OFFSET], conf[CONF_BITMASK]))
    if CONF_CHARGING_STATUS in config:
        conf = config[CONF_CHARGING_STATUS]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DISCHARGING_STATUS in config:
        conf = config[CONF_DISCHARGING_STATUS]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    # // 3300 - 331B Statistical Parameters (Read Only)
    if CONF_MAX_PV_VOLTAGE_TODAY in config:
        conf = config[CONF_MAX_PV_VOLTAGE_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_MIN_PV_VOLTAGE_TODAY in config:
        conf = config[CONF_MIN_PV_VOLTAGE_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_MAX_BATTERY_VOLTAGE_TODAY in config:
        conf = config[CONF_MAX_BATTERY_VOLTAGE_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_MIN_BATTERY_TODAY in config:
        conf = config[CONF_MIN_BATTERY_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONSUMED_ENERGY_TODAY in config:
        conf = config[CONF_CONSUMED_ENERGY_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONSUMED_ENERGY_MONTH in config:
        conf = config[CONF_CONSUMED_ENERGY_MONTH]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONSUMED_ENERGY_YEAR in config:
        conf = config[CONF_CONSUMED_ENERGY_YEAR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONSUMED_ENERGY_TOTAL in config:
        conf = config[CONF_CONSUMED_ENERGY_TOTAL]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_GENERATED_ENERGY_TODAY in config:
        conf = config[CONF_GENERATED_ENERGY_TODAY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_GENERATED_ENERGY_MONTH in config:
        conf = config[CONF_GENERATED_ENERGY_MONTH]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_GENERATED_ENERGY_YEAR in config:
        conf = config[CONF_GENERATED_ENERGY_YEAR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_GENERATED_ENERGY_TOTAL in config:
        conf = config[CONF_GENERATED_ENERGY_TOTAL]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CO2_REDUCTION in config:
        conf = config[CONF_CO2_REDUCTION]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_VOLTAGE in config:
        conf = config[CONF_BATTERY_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_CURRENT in config:
        conf = config[CONF_BATTERY_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_TYPE in config:
        conf = config[CONF_BATTERY_TYPE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_CAPACITY in config:
        conf = config[CONF_BATTERY_CAPACITY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TEMPERATURE_COMPENSATION_COEFFICIENT in config:
        conf = config[CONF_TEMPERATURE_COMPENSATION_COEFFICIENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_HIGH_VOLT_DISCONNECT in config:
        conf = config[CONF_HIGH_VOLT_DISCONNECT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CHARGING_LIMIT_VOLTAGE in config:
        conf = config[CONF_CHARGING_LIMIT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_OVER_VOLTAGE_RECONNECT in config:
        conf = config[CONF_OVER_VOLTAGE_RECONNECT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_EQUALIZATION_VOLTAGE in config:
        conf = config[CONF_EQUALIZATION_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BOOST_VOLTAGE in config:
        conf = config[CONF_BOOST_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_FLOAT_VOLTAGE in config:
        conf = config[CONF_FLOAT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BOOST_RECONNECT_VOLTAGE in config:
        conf = config[CONF_BOOST_RECONNECT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOW_VOLTAGE_RECONNECT in config:
        conf = config[CONF_LOW_VOLTAGE_RECONNECT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_UNDER_VOLTAGE_RECOVER in config:
        conf = config[CONF_UNDER_VOLTAGE_RECOVER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_UNDER_VOLTAGE_WARNING in config:
        conf = config[CONF_UNDER_VOLTAGE_WARNING]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOW_VOLTAGE_DISCONNECT in config:
        conf = config[CONF_LOW_VOLTAGE_DISCONNECT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DISCHARGING_LIMIT_VOLTAGE in config:
        conf = config[CONF_DISCHARGING_LIMIT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_EQUALIZATION_CHARGING_CYCLE in config:
        conf = config[CONF_EQUALIZATION_CHARGING_CYCLE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_TEMPERATURE_WARNING_UPPER_LIMIT in config:
        conf = config[CONF_BATTERY_TEMPERATURE_WARNING_UPPER_LIMIT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_TEMPERATURE_WARNING_LOWER_LIMIT in config:
        conf = config[CONF_BATTERY_TEMPERATURE_WARNING_LOWER_LIMIT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT in config:
        conf = config[CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT_RECOVER in config:
        conf = config[CONF_CONTROLLER_INNER_TEMPERATURE_UPPER_LIMIT_RECOVER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT in config:
        conf = config[CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT_RECOVER in config:
        conf = config[CONF_POWER_COMPONENT_TEMPERATURE_UPPER_LIMIT_RECOVER]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LINE_IMPEDANCE in config:
        conf = config[CONF_LINE_IMPEDANCE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DAY_TIME_THRESHOLD_VOLTAGE in config:
        conf = config[CONF_DAY_TIME_THRESHOLD_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LIGHT_SIGNAL_STARTUP_DELAY_TIME in config:
        conf = config[CONF_LIGHT_SIGNAL_STARTUP_DELAY_TIME]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_NIGHT_TIMETHRESHOLD_VOLTAGE in config:
        conf = config[CONF_NIGHT_TIMETHRESHOLD_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LIGHT_SIGNAL_TURN_OFF_DELAY_TIME in config:
        conf = config[CONF_LIGHT_SIGNAL_TURN_OFF_DELAY_TIME]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LOAD_CONTROL_MODE in config:
        conf = config[CONF_LOAD_CONTROL_MODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_WORKING_TIME_LENGTH_1 in config:
        conf = config[CONF_WORKING_TIME_LENGTH_1]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_WORKING_TIME_LENGTH_2 in config:
        conf = config[CONF_WORKING_TIME_LENGTH_2]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_1_SEC in config:
        conf = config[CONF_TURN_ON_TIMING_1_SEC]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_1_MIN in config:
        conf = config[CONF_TURN_ON_TIMING_1_MIN]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_1_HOUR in config:
        conf = config[CONF_TURN_ON_TIMING_1_HOUR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_1_SEC in config:
        conf = config[CONF_TURN_OFF_TIMING_1_SEC]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_1_MIN in config:
        conf = config[CONF_TURN_OFF_TIMING_1_MIN]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_1_HOUR in config:
        conf = config[CONF_TURN_OFF_TIMING_1_HOUR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_2_SEC in config:
        conf = config[CONF_TURN_ON_TIMING_2_SEC]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_2_MIN in config:
        conf = config[CONF_TURN_ON_TIMING_2_MIN]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_ON_TIMING_2_HOUR in config:
        conf = config[CONF_TURN_ON_TIMING_2_HOUR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_2_SEC in config:
        conf = config[CONF_TURN_OFF_TIMING_2_SEC]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_2_MIN in config:
        conf = config[CONF_TURN_OFF_TIMING_2_MIN]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_TURN_OFF_TIMING_2_HOUR in config:
        conf = config[CONF_TURN_OFF_TIMING_2_HOUR]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))

    if CONF_BACKLIGHT_TIME in config:
        conf = config[CONF_BACKLIGHT_TIME]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_LENGTH_OF_NIGHT in config:
        conf = config[CONF_LENGTH_OF_NIGHT]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_MAIN_POWER_SUPPLY in config:
        conf = config[CONF_MAIN_POWER_SUPPLY]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_RATED_VOLTAGE_CODE in config:
        conf = config[CONF_BATTERY_RATED_VOLTAGE_CODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DEFAULT_LOAD_MODE in config:
        conf = config[CONF_DEFAULT_LOAD_MODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_EQUALIZE_DURATION in config:
        conf = config[CONF_EQUALIZE_DURATION]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BOOST_DURATION in config:
        conf = config[CONF_BOOST_DURATION]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DISCHARGE_PERCENTAGE in config:
        conf = config[CONF_DISCHARGE_PERCENTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_CHARGING_PERCENTAGE in config:
        conf = config[CONF_CHARGING_PERCENTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_BATTERY_MANAGEMENT_MODE in config:
        conf = config[CONF_BATTERY_MANAGEMENT_MODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(var.add_sensor(sens, conf[CONF_MODBUS_FUNCTIONCODE], conf[CONF_ADDRESS],
                              conf[CONF_OFFSET], conf[CONF_BITMASK], conf[CONF_VALUE_TYPE],
                              conf[CONF_SCALE_FACTOR]))
    if CONF_DEVICE_OVERTEMPERATURE in config:
        conf = config[CONF_DEVICE_OVERTEMPERATURE]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE],
                                    conf[CONF_ADDRESS], conf[CONF_OFFSET], conf[CONF_BITMASK]))
    if CONF_IS_NIGHT in config:
        conf = config[CONF_IS_NIGHT]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE],
                                    conf[CONF_ADDRESS], conf[CONF_OFFSET], conf[CONF_BITMASK]))
    if CONF_MANUAL_CONTROL_LOAD in config:
        conf = config[CONF_MANUAL_CONTROL_LOAD]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE],
                                    conf[CONF_ADDRESS], conf[CONF_OFFSET], conf[CONF_BITMASK]))
    if CONF_ENABLE_LOAD_TEST_MODE in config:
        conf = config[CONF_ENABLE_LOAD_TEST_MODE]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE],
                                    conf[CONF_ADDRESS], conf[CONF_OFFSET], conf[CONF_BITMASK]))
    if CONF_FORCE_LOAD_ON in config:
        conf = config[CONF_FORCE_LOAD_ON]
        sens = yield binary_sensor.new_binary_sensor(conf)
        cg.add(var.add_binarysensor(sens, conf[CONF_MODBUS_FUNCTIONCODE],
                                    conf[CONF_ADDRESS], conf[CONF_OFFSET], conf[CONF_BITMASK]))
    cg.add(var.set_sync_rtc(config[CONF_SYNC_RTC]))
    if config.get('sensors'):
        conf = config['sensors']
        for s in conf:
            sens = yield sensor.new_sensor(s)
            cg.add(var.add_sensor(sens, s[CONF_MODBUS_FUNCTIONCODE], s[CONF_ADDRESS],
                                  s[CONF_OFFSET], s[CONF_BITMASK], s[CONF_VALUE_TYPE],
                                  s[CONF_SCALE_FACTOR]))
    if config.get('binary_sensors'):
        conf = config['binary_sensors']
        for s in conf:
            sens = yield binary_sensor.new_binary_sensor(s)
            cg.add(var.add_binarysensor(sens, s[CONF_MODBUS_FUNCTIONCODE],
                                        s[CONF_ADDRESS], s[CONF_OFFSET], s[CONF_BITMASK]))


@coroutine
def build_modbus_registers(config):
    yield cg.build_registry_list(MODBUS_REGISTRY, config)
