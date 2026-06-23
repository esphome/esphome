import esphome.codegen as cg
from esphome.components import number
from esphome.components.opentherm import schema
from esphome.components.opentherm.generate import TYPE_MAP, MessageId
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import StringLiteral
from esphome.types import ConfigType

from .. import CONF_OPENTHERM_BOILER_ID, Boiler, RequestProcessor, opentherm_boiler_ns

BoilerNumber = opentherm_boiler_ns.class_(
    "BoilerNumber", number.Number, RequestProcessor
)

DEPENDENCIES = ["opentherm_boiler"]

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(CONF_OPENTHERM_BOILER_ID): cv.use_id(Boiler)})
    .extend(
        cv.Schema(
            {
                cv.Optional(key): number.number_schema(
                    BoilerNumber,
                    icon=schema_.icon or cv.UNDEFINED,
                    device_class=schema_.device_class or cv.UNDEFINED,
                    unit_of_measurement=schema_.unit_of_measurement or cv.UNDEFINED,
                )
                for key, schema_ in schema.SENSORS.items()
            }
        )
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def number_config(schema_: schema.SensorSchema) -> dict:
    if schema_.message_data == "u16":
        return {"min_value": 0.0, "max_value": 65535.0, "step": 1.0}
    if schema_.message_data == "s16":
        return {"min_value": -32768.0, "max_value": 32767.0, "step": 1.0}
    if schema_.message_data.startswith("u8_"):
        return {"min_value": 0.0, "max_value": 255.0, "step": 1.0}
    if schema_.message_data.startswith("s8_"):
        return {"min_value": -128.0, "max_value": 127.0, "step": 1.0}
    if schema_.message_data == "f88":
        return {"min_value": -127.0, "max_value": 127.0, "step": 1.0}
    raise NotImplementedError(f"Unsupported message data type: {schema_.message_data}")


async def to_code(config: ConfigType) -> None:
    boiler = await cg.get_variable(config[CONF_OPENTHERM_BOILER_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue

        id = conf[CONF_ID]
        if id and id.type == BoilerNumber:
            schema_ = schema.SENSORS[key]
            accessor = TYPE_MAP[schema_.message_data]
            message_id = getattr(MessageId, schema_.message)

            entity = await number.new_number(
                conf,
                cg.TemplateArguments(accessor),
                **number_config(schema_),
            )
            cg.add(entity.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(boiler.register_request_processor(message_id, entity))
