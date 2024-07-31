import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP

from .. import CONF_OPENTHERM_ID, OpenthermHub, opentherm_ns, input, schema, generate

OpenthermNumber = opentherm_ns.class_("OpenthermNumber", number.Number, cg.Component)

DEPENDENCIES = ["opentherm", "number"]

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermHub),
    })
    .extend({
        cv.Optional(key): number.number_schema(
            OpenthermNumber.template(generate.get_type(entity))
            if generate.get_type(entity) is not None else
            OpenthermNumber,
            icon=entity["icon"] if "icon" in entity else number._UNDEF,
            device_class=entity["device_class"] if "device_class" in entity else number._UNDEF,
            unit_of_measurement=entity["unit_of_measurement"] if "unit_of_measurement" in entity else number._UNDEF,
        )
        .extend(generate.opentherm_schema(entity))
        .extend(input.input_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
        for key, entity in schema.INPUTS.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    for key, conf in config.items():
        if isinstance(conf, dict):
            var = generate.create_opentherm_component(conf)

            await cg.register_component(var, conf)
            await number.register_number(var, conf,
                                         min_value=conf[CONF_MIN_VALUE],
                                         max_value=conf[CONF_MAX_VALUE],
                                         step=conf[CONF_STEP])

            hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
            cg.add(hub.register_component(var))
