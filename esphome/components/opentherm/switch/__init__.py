import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from .. import CONF_OPENTHERM_ID, OpenthermHub, opentherm_ns, schema, generate

OpenthermSwitch = opentherm_ns.class_("OpenthermSwitch", switch.Switch, cg.Component)

DEPENDENCIES = ["opentherm", "switch"]

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermHub),
    })
    .extend({
        cv.Optional(key): switch.switch_schema(
            OpenthermSwitch.template(generate.get_type(entity))
            if generate.get_type(entity) is not None else
            OpenthermSwitch,
            device_class=entity["device_class"] if "device_class" in entity else switch._UNDEF,
            icon=entity["icon"] if "icon" in entity else switch._UNDEF,
            default_restore_mode=entity["default_restore_mode"] if "default_restore_mode" in entity else switch._UNDEF,
        )
        .extend(generate.opentherm_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
        for key, entity in schema.SWITCHES.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    for key, conf in config.items():
        if isinstance(conf, dict):
            var = generate.create_opentherm_component(conf)
            await cg.register_component(var, conf)
            await switch.register_switch(var, conf)

            hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
            cg.add(hub.register_component(var))
