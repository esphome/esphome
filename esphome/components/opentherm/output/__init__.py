import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output

from .. import CONF_OPENTHERM_ID, OpenthermHub, opentherm_ns, input, schema, generate

OpenthermOutput = opentherm_ns.class_("OpenthermOutput", output.FloatOutput, cg.Component)

DEPENDENCIES = ["opentherm", "output"]

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermHub),
    })
    .extend({
        cv.Optional(key): (
            output.FLOAT_OUTPUT_SCHEMA
            .extend({
                cv.GenerateID(): cv.declare_id(OpenthermOutput.template(generate.get_type(entity))
                                               if generate.get_type(entity) is not None else
                                               OpenthermOutput),
            })
            .extend(generate.opentherm_schema(entity))
            .extend(input.input_schema(entity))
            .extend(cv.COMPONENT_SCHEMA)
        )
        for key, entity in schema.INPUTS.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    for key, conf in config.items():
        if isinstance(conf, dict):
            var = generate.create_opentherm_component(conf)

            await cg.register_component(var, conf)
            await output.register_output(var, conf)

            hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
            cg.add(hub.register_component(var))
