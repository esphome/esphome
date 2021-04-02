import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    ICON_EMPTY,
    UNIT_WATT_HOURS,
    DEVICE_CLASS_POWER,
)
from . import DSMR, CONF_DSMR_ID

AUTO_LOAD = ["dsmr"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DSMR_ID): cv.use_id(DSMR),
        cv.Optional("energy_delivered_tariff1"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("energy_delivered_tariff2"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("energy_returned_tariff1"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("energy_returned_tariff2"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_delivered"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("power_returned"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_threshold"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_switch_position"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_failures"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_long_failures"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_sags_l1"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_sags_l2"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_sags_l3"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_swells_l1"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_swells_l2"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
        ),
        cv.Optional("electricity_swells_l3"): sensor.sensor_schema(
            UNIT_WATT_HOURS, ICON_EMPTY, 3, DEVICE_CLASS_POWER
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
