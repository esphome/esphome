import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.opentherm import schema
from esphome.components.opentherm.generate import TYPE_MAP, MessageId
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import StringLiteral
from esphome.types import ConfigType

from .. import CONF_OPENTHERM_BOILER_ID, Boiler, RequestProcessor, opentherm_boiler_ns

BoilerSensor = opentherm_boiler_ns.class_(
    "BoilerSensor", sensor.Sensor, RequestProcessor
)

DEPENDENCIES = ["opentherm_boiler"]


def sensor_config(schema_: schema.InputSchema) -> dict:
    return {
        "unit_of_measurement": schema_.unit_of_measurement,
        "icon": schema_.icon or cv.UNDEFINED,
        "accuracy_decimals": 2 if schema_.message_data == "f88" else 0,
    }


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(CONF_OPENTHERM_BOILER_ID): cv.use_id(Boiler)})
    .extend(
        cv.Schema(
            {
                cv.Optional(key): sensor.sensor_schema(
                    BoilerSensor, **sensor_config(schema_)
                )
                for key, schema_ in schema.INPUTS.items()
            }
        )
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    boiler = await cg.get_variable(config[CONF_OPENTHERM_BOILER_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue

        id = conf[CONF_ID]
        if id and id.type == BoilerSensor:
            schema_ = schema.INPUTS[key]
            accessor = TYPE_MAP[schema_.message_data]
            message_id = getattr(MessageId, schema_.message)

            entity = await sensor.new_sensor(conf, cg.TemplateArguments(accessor))
            cg.add(entity.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(boiler.register_request_processor(message_id, entity))
