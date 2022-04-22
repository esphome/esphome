import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ICON_CURRENT_AC,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    CONF_BUS_VOLTAGE,
    CONF_BATTERY_VOLTAGE,
)
from .. import PIPSOLAR_COMPONENT_SCHEMA, CONF_PIPSOLAR_ID

DEPENDENCIES = ["uart"]

# QPIRI sensors
CONF_GRID_RATING_VOLTAGE = "grid_rating_voltage"
CONF_GRID_RATING_CURRENT = "grid_rating_current"
CONF_AC_OUTPUT_RATING_VOLTAGE = "ac_output_rating_voltage"
CONF_AC_OUTPUT_RATING_FREQUENCY = "ac_output_rating_frequency"
CONF_AC_OUTPUT_RATING_CURRENT = "ac_output_rating_current"
CONF_AC_OUTPUT_RATING_APPARENT_POWER = "ac_output_rating_apparent_power"
CONF_AC_OUTPUT_RATING_ACTIVE_POWER = "ac_output_rating_active_power"
CONF_BATTERY_RATING_VOLTAGE = "battery_rating_voltage"
CONF_BATTERY_RECHARGE_VOLTAGE = "battery_recharge_voltage"
CONF_BATTERY_UNDER_VOLTAGE = "battery_under_voltage"
CONF_BATTERY_BULK_VOLTAGE = "battery_bulk_voltage"
CONF_BATTERY_FLOAT_VOLTAGE = "battery_float_voltage"
CONF_BATTERY_TYPE = "battery_type"
CONF_CURRENT_MAX_AC_CHARGING_CURRENT = "current_max_ac_charging_current"
CONF_CURRENT_MAX_CHARGING_CURRENT = "current_max_charging_current"
CONF_INPUT_VOLTAGE_RANGE = "input_voltage_range"
CONF_OUTPUT_SOURCE_PRIORITY = "output_source_priority"
CONF_CHARGER_SOURCE_PRIORITY = "charger_source_priority"
CONF_PARALLEL_MAX_NUM = "parallel_max_num"
CONF_MACHINE_TYPE = "machine_type"
CONF_TOPOLOGY = "topology"
CONF_OUTPUT_MODE = "output_mode"
CONF_BATTERY_REDISCHARGE_VOLTAGE = "battery_redischarge_voltage"
CONF_PV_OK_CONDITION_FOR_PARALLEL = "pv_ok_condition_for_parallel"
CONF_PV_POWER_BALANCE = "pv_power_balance"

CONF_GRID_VOLTAGE = "grid_voltage"
CONF_GRID_FREQUENCY = "grid_frequency"
CONF_AC_OUTPUT_VOLTAGE = "ac_output_voltage"
CONF_AC_OUTPUT_FREQUENCY = "ac_output_frequency"
CONF_AC_OUTPUT_APPARENT_POWER = "ac_output_apparent_power"
CONF_AC_OUTPUT_ACTIVE_POWER = "ac_output_active_power"
CONF_OUTPUT_LOAD_PERCENT = "output_load_percent"
CONF_BATTERY_CHARGING_CURRENT = "battery_charging_current"
CONF_BATTERY_CAPACITY_PERCENT = "battery_capacity_percent"
CONF_INVERTER_HEAT_SINK_TEMPERATURE = "inverter_heat_sink_temperature"
CONF_PV_INPUT_CURRENT_FOR_BATTERY = "pv_input_current_for_battery"
CONF_PV_INPUT_VOLTAGE = "pv_input_voltage"
CONF_BATTERY_VOLTAGE_SCC = "battery_voltage_scc"
CONF_BATTERY_DISCHARGE_CURRENT = "battery_discharge_current"
CONF_ADD_SBU_PRIORITY_VERSION = "add_sbu_priority_version"
CONF_CONFIGURATION_STATUS = "configuration_status"
CONF_SCC_FIRMWARE_VERSION = "scc_firmware_version"
CONF_BATTERY_VOLTAGE_OFFSET_FOR_FANS_ON = "battery_voltage_offset_for_fans_on"
CONF_EEPROM_VERSION = "eeprom_version"
CONF_PV_CHARGING_POWER = "pv_charging_power"

TYPES = {
    CONF_GRID_RATING_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_GRID_RATING_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_AC_OUTPUT_RATING_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_AC_OUTPUT_RATING_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=1,
    ),
    CONF_AC_OUTPUT_RATING_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_AC_OUTPUT_RATING_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=1,
    ),
    CONF_AC_OUTPUT_RATING_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
    ),
    CONF_BATTERY_RATING_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_RECHARGE_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_UNDER_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_BULK_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_FLOAT_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_TYPE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_CURRENT_MAX_AC_CHARGING_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_CURRENT_MAX_CHARGING_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_INPUT_VOLTAGE_RANGE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_OUTPUT_SOURCE_PRIORITY: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_CHARGER_SOURCE_PRIORITY: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_PARALLEL_MAX_NUM: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_MACHINE_TYPE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_TOPOLOGY: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_OUTPUT_MODE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_BATTERY_REDISCHARGE_VOLTAGE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_PV_OK_CONDITION_FOR_PARALLEL: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_PV_POWER_BALANCE: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_GRID_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_GRID_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=1,
    ),
    CONF_AC_OUTPUT_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_AC_OUTPUT_FREQUENCY: sensor.sensor_schema(
        unit_of_measurement=UNIT_HERTZ,
        icon=ICON_CURRENT_AC,
        accuracy_decimals=1,
    ),
    CONF_AC_OUTPUT_APPARENT_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT_AMPS,
        accuracy_decimals=1,
    ),
    CONF_AC_OUTPUT_ACTIVE_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
    ),
    CONF_OUTPUT_LOAD_PERCENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
    ),
    CONF_BUS_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_CHARGING_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_BATTERY_CAPACITY_PERCENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=1,
    ),
    CONF_INVERTER_HEAT_SINK_TEMPERATURE: sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_PV_INPUT_CURRENT_FOR_BATTERY: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_PV_INPUT_VOLTAGE: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_VOLTAGE_SCC: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_BATTERY_DISCHARGE_CURRENT: sensor.sensor_schema(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_CURRENT,
    ),
    CONF_BATTERY_VOLTAGE_OFFSET_FOR_FANS_ON: sensor.sensor_schema(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_VOLTAGE,
    ),
    CONF_EEPROM_VERSION: sensor.sensor_schema(
        accuracy_decimals=1,
    ),
    CONF_PV_CHARGING_POWER: sensor.sensor_schema(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
    ),
}

CONFIG_SCHEMA = PIPSOLAR_COMPONENT_SCHEMA.extend(
    {cv.Optional(type): schema for type, schema in TYPES.items()}
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PIPSOLAR_ID])

    for type, _ in TYPES.items():
        if type in config:
            conf = config[type]
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(paren, f"set_{type}")(sens))
