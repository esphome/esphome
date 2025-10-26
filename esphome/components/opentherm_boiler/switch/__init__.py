import esphome.codegen as cg
from esphome.components import switch
from esphome.components.opentherm import schema
from esphome.components.opentherm.generate import TYPE_MAP, MessageId
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import StringLiteral
from esphome.types import ConfigType

from .. import CONF_OPENTHERM_BOILER_ID, Boiler, RequestProcessor, opentherm_boiler_ns

BoilerSwitch = opentherm_boiler_ns.class_(
    "BoilerSwitch", switch.Switch, RequestProcessor
)

DEPENDENCIES = ["opentherm_boiler"]

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(CONF_OPENTHERM_BOILER_ID): cv.use_id(Boiler)})
    .extend(
        cv.Schema(
            {
                cv.Optional(key): switch.switch_schema(
                    BoilerSwitch,
                    icon=schema_.icon or cv.UNDEFINED,
                )
                for key, schema_ in schema.BINARY_SENSORS.items()
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
        if id and id.type == BoilerSwitch:
            schema_ = schema.BINARY_SENSORS[key]
            accessor = TYPE_MAP[schema_.message_data]
            message_id = getattr(MessageId, schema_.message)

            entity = await switch.new_switch(
                conf,
                cg.TemplateArguments(accessor),
            )
            cg.add(entity.set_id(StringLiteral(f"{key} ({id})")))
            cg.add(boiler.register_request_processor(message_id, entity))
