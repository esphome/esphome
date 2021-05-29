import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ICON_EMPTY,
    UNIT_EMPTY,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_AMPERE,
    UNIT_WATT_HOURS,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
)
from . import DSMR, CONF_DSMR_ID

AUTO_LOAD = ["dsmr"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DSMR_ID): cv.use_id(DSMR),
        cv.Optional("energy_delivered_tariff1"): sensor.sensor_schema(
            "kWh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("energy_delivered_tariff2"): sensor.sensor_schema(
            "kWh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("energy_returned_tariff1"): sensor.sensor_schema(
            "kWh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("energy_returned_tariff2"): sensor.sensor_schema(
            "kWh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("total_imported_energy"): sensor.sensor_schema(
            "kvarh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("total_exported_energy"): sensor.sensor_schema(
            "kvarh", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("power_delivered"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_returned"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_delivered"): sensor.sensor_schema(
            "kvar", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("reactive_power_returned"): sensor.sensor_schema(
            "kvar", ICON_EMPTY, 3, DEVICE_CLASS_ENERGY
        ),
        cv.Optional("electricity_threshold"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 3, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_switch_position"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 3, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_failures"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_long_failures"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_sags_l1"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_sags_l2"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_sags_l3"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_swells_l1"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_swells_l2"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("electricity_swells_l3"): sensor.sensor_schema(
            UNIT_EMPTY, ICON_EMPTY, 0, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("current_l1"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT
        ),
        cv.Optional("current_l2"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT
        ),
        cv.Optional("current_l3"): sensor.sensor_schema(
            UNIT_AMPERE, ICON_EMPTY, 1, DEVICE_CLASS_CURRENT
        ),
        cv.Optional("power_delivered_l1"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_delivered_l2"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_delivered_l3"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_returned_l1"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_returned_l2"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_returned_l3"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_delivered_l1"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_delivered_l2"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_delivered_l3"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_returned_l1"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_returned_l2"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("reactive_power_returned_l3"): sensor.sensor_schema(
            UNIT_WATT, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("voltage_l1"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional("voltage_l2"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional("voltage_l3"): sensor.sensor_schema(
            UNIT_VOLT, ICON_EMPTY, 1, DEVICE_CLASS_VOLTAGE
        ),
        cv.Optional("gas_delivered"): sensor.sensor_schema(
            "m³", ICON_EMPTY, 3, DEVICE_CLASS_EMPTY
        ),
        cv.Optional("gas_delivered_be"): sensor.sensor_schema(
            "m³", ICON_EMPTY, 3, DEVICE_CLASS_EMPTY
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_DSMR_ID])

    sensors = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == sensor.Sensor:
            s = yield sensor.new_sensor(conf)
            cg.add(getattr(hub, f"set_{key}")(s))
            sensors.append(f"F({key})")

    cg.add_define("DSMR_SENSOR_LIST(F, sep)", cg.RawExpression(" sep ".join(sensors)))
