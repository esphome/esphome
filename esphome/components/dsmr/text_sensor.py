import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import Dsmr, CONF_DSMR_ID

AUTO_LOAD = ["dsmr"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DSMR_ID): cv.use_id(Dsmr),
        cv.Optional("identification"): text_sensor.text_sensor_schema(),
        cv.Optional("p1_version"): text_sensor.text_sensor_schema(),
        cv.Optional("p1_version_be"): text_sensor.text_sensor_schema(),
        cv.Optional("timestamp"): text_sensor.text_sensor_schema(),
        cv.Optional("electricity_tariff"): text_sensor.text_sensor_schema(),
        cv.Optional("electricity_failure_log"): text_sensor.text_sensor_schema(),
        cv.Optional("message_short"): text_sensor.text_sensor_schema(),
        cv.Optional("message_long"): text_sensor.text_sensor_schema(),
        cv.Optional("gas_equipment_id"): text_sensor.text_sensor_schema(),
        cv.Optional("thermal_equipment_id"): text_sensor.text_sensor_schema(),
        cv.Optional("water_equipment_id"): text_sensor.text_sensor_schema(),
        cv.Optional("sub_equipment_id"): text_sensor.text_sensor_schema(),
        cv.Optional("gas_delivered_text"): text_sensor.text_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DSMR_ID])

    text_sensors = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == text_sensor.TextSensor:
            var = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(hub, f"set_{key}")(var))
            text_sensors.append(f"F({key})")

    if text_sensors:
        cg.add_define(
            "DSMR_TEXT_SENSOR_LIST(F, sep)",
            cg.RawExpression(" sep ".join(text_sensors)),
        )
