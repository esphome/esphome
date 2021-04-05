import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_POWER,
    ICON_EMPTY,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_EMPTY,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    CONF_PIPSOLAR_ID,
    CONF_BUS_VOLTAGE,
    CONF_BATTERY_VOLTAGE
)
from . import PipsolarComponent, pipsolar_ns

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

pipsolar_sensor_ns = cg.esphome_ns.namespace("pipsolarsensor")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(pipsolar_sensor_ns),
        cv.Required(CONF_PIPSOLAR_ID): cv.use_id(PipsolarComponent),
        # QPIRI sensors
        cv.Optional(CONF_GRID_RATING_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_GRID_RATING_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_RATING_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_RATING_FREQUENCY): sensor.sensor_schema(
            UNIT_HERTZ, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_RATING_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_RATING_APPARENT_POWER): sensor.sensor_schema(
            UNIT_VOLT_AMPS, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_RATING_ACTIVE_POWER): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_RATING_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_RECHARGE_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_UNDER_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_BULK_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_FLOAT_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_TYPE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_CURRENT_MAX_AC_CHARGING_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_CURRENT_MAX_CHARGING_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_INPUT_VOLTAGE_RANGE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_OUTPUT_SOURCE_PRIORITY): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_CHARGER_SOURCE_PRIORITY): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PARALLEL_MAX_NUM): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_MACHINE_TYPE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_TOPOLOGY): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_OUTPUT_MODE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_REDISCHARGE_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PV_OK_CONDITION_FOR_PARALLEL): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PV_POWER_BALANCE): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        # QPIGS sensors
        cv.Optional(CONF_GRID_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_GRID_FREQUENCY): sensor.sensor_schema(
            UNIT_HERTZ, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_FREQUENCY): sensor.sensor_schema(
            UNIT_HERTZ, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_APPARENT_POWER): sensor.sensor_schema(
            UNIT_VOLT_AMPS, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_AC_OUTPUT_ACTIVE_POWER): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_OUTPUT_LOAD_PERCENT): sensor.sensor_schema(
            UNIT_PERCENT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BUS_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_CHARGING_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_CAPACITY_PERCENT): sensor.sensor_schema(
            UNIT_PERCENT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_INVERTER_HEAT_SINK_TEMPERATURE): sensor.sensor_schema(
            UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PV_INPUT_CURRENT_FOR_BATTERY): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PV_INPUT_VOLTAGE): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE_SCC): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_DISCHARGE_CURRENT): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_BATTERY_VOLTAGE_OFFSET_FOR_FANS_ON): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_EEPROM_VERSION): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
        cv.Optional(CONF_PV_CHARGING_POWER): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 1, DEVICE_CLASS_POWER
        ),
    }
)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_PIPSOLAR_ID])
    # QPIRI sensors
    if CONF_GRID_RATING_VOLTAGE in config:
        conf = config[CONF_GRID_RATING_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_grid_rating_voltage_sensor(sens))
    if CONF_GRID_RATING_CURRENT in config:
        conf = config[CONF_GRID_RATING_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_grid_rating_current_sensor(sens))
    if CONF_AC_OUTPUT_RATING_VOLTAGE in config:
        conf = config[CONF_AC_OUTPUT_RATING_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_rating_voltage_sensor(sens))
    if CONF_AC_OUTPUT_RATING_FREQUENCY in config:
        conf = config[CONF_AC_OUTPUT_RATING_FREQUENCY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_rating_frequency_sensor(sens))
    if CONF_AC_OUTPUT_RATING_CURRENT in config:
        conf = config[CONF_AC_OUTPUT_RATING_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_rating_current_sensor(sens))
    if CONF_AC_OUTPUT_RATING_APPARENT_POWER in config:
        conf = config[CONF_AC_OUTPUT_RATING_APPARENT_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_rating_apparent_power_sensor(sens))
    if CONF_AC_OUTPUT_RATING_ACTIVE_POWER in config:
        conf = config[CONF_AC_OUTPUT_RATING_ACTIVE_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_rating_active_power_sensor(sens))
    if CONF_BATTERY_RATING_VOLTAGE in config:
        conf = config[CONF_BATTERY_RATING_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_rating_voltage_sensor(sens))
    if CONF_BATTERY_RECHARGE_VOLTAGE in config:
        conf = config[CONF_BATTERY_RECHARGE_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_recharge_voltage_sensor(sens))
    if CONF_BATTERY_UNDER_VOLTAGE in config:
        conf = config[CONF_BATTERY_UNDER_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_under_voltage_sensor(sens))
    if CONF_BATTERY_BULK_VOLTAGE in config:
        conf = config[CONF_BATTERY_BULK_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_bulk_voltage_sensor(sens))
    if CONF_BATTERY_FLOAT_VOLTAGE in config:
        conf = config[CONF_BATTERY_FLOAT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_float_voltage_sensor(sens))
    if CONF_BATTERY_TYPE in config:
        conf = config[CONF_BATTERY_TYPE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_type_sensor(sens))
    if CONF_CURRENT_MAX_AC_CHARGING_CURRENT in config:
        conf = config[CONF_CURRENT_MAX_AC_CHARGING_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_current_max_ac_charging_current_sensor(sens))
    if CONF_CURRENT_MAX_CHARGING_CURRENT in config:
        conf = config[CONF_CURRENT_MAX_CHARGING_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_current_max_charging_current_sensor(sens))
    if CONF_INPUT_VOLTAGE_RANGE in config:
        conf = config[CONF_INPUT_VOLTAGE_RANGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_input_voltage_range_sensor(sens))
    if CONF_OUTPUT_SOURCE_PRIORITY in config:
        conf = config[CONF_OUTPUT_SOURCE_PRIORITY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_output_source_priority_sensor(sens))
    if CONF_CHARGER_SOURCE_PRIORITY in config:
        conf = config[CONF_CHARGER_SOURCE_PRIORITY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_charger_source_priority_sensor(sens))
    if CONF_PARALLEL_MAX_NUM in config:
        conf = config[CONF_PARALLEL_MAX_NUM]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_parallel_max_num_sensor(sens))
    if CONF_MACHINE_TYPE in config:
        conf = config[CONF_MACHINE_TYPE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_machine_type_sensor(sens))
    if CONF_TOPOLOGY in config:
        conf = config[CONF_TOPOLOGY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_topology_sensor(sens))
    if CONF_OUTPUT_MODE in config:
        conf = config[CONF_OUTPUT_MODE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_output_mode_sensor(sens))
    if CONF_BATTERY_REDISCHARGE_VOLTAGE in config:
        conf = config[CONF_BATTERY_REDISCHARGE_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_redischarge_voltage_sensor(sens))
    if CONF_PV_OK_CONDITION_FOR_PARALLEL in config:
        conf = config[CONF_PV_OK_CONDITION_FOR_PARALLEL]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_pv_ok_condition_for_parallel_sensor(sens))
    if CONF_PV_POWER_BALANCE in config:
        conf = config[CONF_PV_POWER_BALANCE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_pv_power_balance_sensor(sens))
    # QPIGS sensors
    if CONF_GRID_VOLTAGE in config:
        conf = config[CONF_GRID_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_grid_voltage_sensor(sens))
    if CONF_GRID_FREQUENCY in config:
        conf = config[CONF_GRID_FREQUENCY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_grid_frequency_sensor(sens))
    if CONF_AC_OUTPUT_VOLTAGE in config:
        conf = config[CONF_AC_OUTPUT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_voltage_sensor(sens))
    if CONF_AC_OUTPUT_FREQUENCY in config:
        conf = config[CONF_AC_OUTPUT_FREQUENCY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_frequency_sensor(sens))
    if CONF_AC_OUTPUT_APPARENT_POWER in config:
        conf = config[CONF_AC_OUTPUT_APPARENT_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_apparent_power_sensor(sens))
    if CONF_AC_OUTPUT_ACTIVE_POWER in config:
        conf = config[CONF_AC_OUTPUT_ACTIVE_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_ac_output_active_power_sensor(sens))
    if CONF_OUTPUT_LOAD_PERCENT in config:
        conf = config[CONF_OUTPUT_LOAD_PERCENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_output_load_percent_sensor(sens))
    if CONF_BUS_VOLTAGE in config:
        conf = config[CONF_BUS_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_bus_voltage_sensor(sens))
    if CONF_BATTERY_VOLTAGE in config:
        conf = config[CONF_BATTERY_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_voltage_sensor(sens))
    if CONF_BATTERY_CHARGING_CURRENT in config:
        conf = config[CONF_BATTERY_CHARGING_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_charging_current_sensor(sens))
    if CONF_BATTERY_CAPACITY_PERCENT in config:
        conf = config[CONF_BATTERY_CAPACITY_PERCENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_capacity_percent_sensor(sens))
    if CONF_INVERTER_HEAT_SINK_TEMPERATURE in config:
        conf = config[CONF_INVERTER_HEAT_SINK_TEMPERATURE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_inverter_heat_sink_temperature_sensor(sens))
    if CONF_PV_INPUT_CURRENT_FOR_BATTERY in config:
        conf = config[CONF_PV_INPUT_CURRENT_FOR_BATTERY]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_pv_input_current_for_battery_sensor(sens))
    if CONF_PV_INPUT_VOLTAGE in config:
        conf = config[CONF_PV_INPUT_VOLTAGE]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_pv_input_voltage_sensor(sens))
    if CONF_BATTERY_VOLTAGE_SCC in config:
        conf = config[CONF_BATTERY_VOLTAGE_SCC]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_voltage_scc_sensor(sens))
    if CONF_BATTERY_DISCHARGE_CURRENT in config:
        conf = config[CONF_BATTERY_DISCHARGE_CURRENT]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_discharge_current_sensor(sens))
    if CONF_BATTERY_VOLTAGE_OFFSET_FOR_FANS_ON in config:
        conf = config[CONF_BATTERY_VOLTAGE_OFFSET_FOR_FANS_ON]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_battery_voltage_offset_for_fans_on_sensor(sens))
    if CONF_EEPROM_VERSION in config:
        conf = config[CONF_EEPROM_VERSION]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_eeprom_version_sensor(sens))
    if CONF_PV_CHARGING_POWER in config:
        conf = config[CONF_PV_CHARGING_POWER]
        sens = yield sensor.new_sensor(conf)
        cg.add(paren.set_pv_charging_power_sensor(sens))
