import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLTAGE,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CUBIC_METER,
    UNIT_EMPTY,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
    UNIT_KILOVOLT_AMPS_REACTIVE,
    UNIT_VOLT,
)
from . import Dsmr, CONF_DSMR_ID

AUTO_LOAD = ["dsmr"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DSMR_ID): cv.use_id(Dsmr),
        cv.Optional("energy_delivered_lux"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff1"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_delivered_tariff2"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_lux"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff1"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("energy_returned_tariff2"): sensor.sensor_schema(
            UNIT_KILOWATT_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("total_imported_energy"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_NONE,
        ),
        cv.Optional("total_exported_energy"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE_HOURS,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_NONE,
        ),
        cv.Optional("power_delivered"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_returned"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("reactive_power_delivered"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_POWER,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_POWER,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("electricity_threshold"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 3, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_switch_position"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 3, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_failures"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_long_failures"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_sags_l1"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_sags_l2"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_sags_l3"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_swells_l1"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_swells_l2"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("electricity_swells_l3"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY, STATE_CLASS_NONE
        ),
        cv.Optional("current_l1"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("current_l2"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("current_l3"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_delivered_l1"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_delivered_l2"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_delivered_l3"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_returned_l1"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_returned_l2"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("power_returned_l3"): sensor.sensor_schema(
            UNIT_KILOWATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("reactive_power_delivered_l1"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l2"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_delivered_l3"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l1"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l2"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("reactive_power_returned_l3"): sensor.sensor_schema(
            UNIT_KILOVOLT_AMPS_REACTIVE,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_EMPTY,
            STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voltage_l1"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_NONE
        ),
        cv.Optional("voltage_l2"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_NONE
        ),
        cv.Optional("voltage_l3"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE, STATE_CLASS_NONE
        ),
        cv.Optional("gas_delivered"): sensor.sensor_schema(
            UNIT_CUBIC_METER,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_GAS,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("gas_delivered_be"): sensor.sensor_schema(
            UNIT_CUBIC_METER,
            ICON_EMPTY,
            3,
            DEVICE_CLASS_GAS,
            STATE_CLASS_TOTAL_INCREASING,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DSMR_ID])

    sensors = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == sensor.Sensor:
            s = await sensor.new_sensor(conf)
            cg.add(getattr(hub, f"set_{key}")(s))
            sensors.append(f"F({key})")

    if sensors:
        cg.add_define(
            "DSMR_SENSOR_LIST(F, sep)", cg.RawExpression(" sep ".join(sensors))
        )
