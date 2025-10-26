import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.opentherm import schema
from esphome.components.opentherm.generate import TYPE_MAP, MessageId
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import StringLiteral
from esphome.types import ConfigType

from .. import CONF_OPENTHERM_BOILER_ID, Boiler, RequestProcessor, opentherm_boiler_ns

BoilerBinarySensor = opentherm_boiler_ns.class_(
    "BoilerBinarySensor", binary_sensor.BinarySensor, RequestProcessor
)

DEPENDENCIES = ["opentherm_boiler"]

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(CONF_OPENTHERM_BOILER_ID): cv.use_id(Boiler)})
    .extend(
        cv.Schema(
            {
                cv.Optional(key): binary_sensor.binary_sensor_schema(BoilerBinarySensor)
                for key in schema.SWITCHES
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
        if id and id.type == BoilerBinarySensor:
            schema_ = schema.SWITCHES[key]
            accessor = TYPE_MAP[schema_.message_data]
            message_id = getattr(MessageId, schema_.message)

            entity = await binary_sensor.new_binary_sensor(
                conf, cg.TemplateArguments(accessor)
            )
            cg.add(entity.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(boiler.register_request_processor(message_id, entity))
