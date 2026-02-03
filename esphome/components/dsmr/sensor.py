import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_WATER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CUBIC_METER,
    UNIT_HERTZ,
    UNIT_KILOVOLT_AMPS,
    UNIT_KILOVOLT_AMPS_REACTIVE,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_SECOND,
    UNIT_VOLT,
)

from . import CONF_DSMR_ID, Dsmr

AUTO_LOAD = ["dsmr"]
UNIT_GIGA_JOULE = "GJ"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DSMR_ID): cv.use_id(Dsmr),
        cv.Optional("energy_delivered_lux"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff4"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_lux"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff4"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff1_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff2_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff1_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff2_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("total_imported_energy"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_delivered_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_delivered_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_delivered_tariff3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_delivered_tariff4"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("total_exported_energy"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_returned_tariff1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_returned_tariff2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_returned_tariff3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("reactive_energy_returned_tariff4"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            accuracy_decimals=3,
        ),
        cv.Optional("power_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_ch"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_threshold"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_switch_position"): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_failures"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_long_failures"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_sags_l1"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_sags_l2"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_sags_l3"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_swells_l1"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_swells_l2"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_swells_l3"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_time_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_time_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_time_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_sag_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_time_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_time_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_time_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_swell_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_n"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_sum"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_fuse_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_fuse_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("current_fuse_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_delivered_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("power_returned_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_avg_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_avg_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_avg_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage"): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("frequency"): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("abs_power"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("gas_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_GAS,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("gas_delivered_be"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_GAS,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("water_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_WATER,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("thermal_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_GIGA_JOULE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("sub_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METER,
            accuracy_decimals=3,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("gas_device_type"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("gas_valve_position"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("thermal_device_type"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("thermal_valve_position"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("water_device_type"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("water_valve_position"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("sub_device_type"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("sub_valve_position"): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_delivery_power"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_delivery_power_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_delivery_power_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_delivery_power_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_return_power"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_return_power_l1"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_return_power_l2"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("apparent_return_power_l3"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("active_demand_power"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("active_demand_abs"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "active_energy_import_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "active_energy_export_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "reactive_energy_import_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "reactive_energy_export_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "apparent_energy_import_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "apparent_energy_export_current_average_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("active_energy_import_last_completed_demand"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("active_energy_export_last_completed_demand"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "reactive_energy_import_last_completed_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "reactive_energy_export_last_completed_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS_REACTIVE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_REACTIVE_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "apparent_energy_import_last_completed_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "apparent_energy_export_last_completed_demand"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOVOLT_AMPS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_APPARENT_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "active_energy_import_maximum_demand_running_month"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(
            "active_energy_import_maximum_demand_last_13_months"
        ): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fw_core_version"): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fw_module_version"): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DSMR_ID])

    sensors = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf[CONF_ID]
        if id and id.type == sensor.Sensor:
            sens = await sensor.new_sensor(conf)
            cg.add(getattr(hub, f"set_{key}")(sens))
            sensors.append(f"F({key})")

    if sensors:
        cg.add_define(
            "DSMR_SENSOR_LIST(F, sep)", cg.RawExpression(" sep ".join(sensors))
        )
