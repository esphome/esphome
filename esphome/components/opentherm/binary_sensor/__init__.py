import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import schema, opentherm_ns, OpenthermHub, CONF_OPENTHERM_ID, generate

OpenthermBinarySensor = opentherm_ns.class_("OpenthermBinarySensor", binary_sensor.BinarySensor, cg.Component)

DEPENDENCIES = ["opentherm", "binary_sensor"]

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermHub),
    })
    .extend({
        cv.Optional(key): binary_sensor.binary_sensor_schema(
            OpenthermBinarySensor.template(generate.get_type(entity))
            if generate.get_type(entity) is not None else
            OpenthermBinarySensor,
            device_class=entity["device_class"] if "device_class" in entity else binary_sensor._UNDEF,
            icon=entity["icon"] if "icon" in entity else binary_sensor._UNDEF,
        )
        .extend(generate.opentherm_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
        for key, entity in schema.BINARY_SENSORS.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    for key, conf in config.items():
        if isinstance(conf, dict):
            var: cg.MockObjClass = generate.create_opentherm_component(conf)

            await cg.register_component(var, conf)
            await binary_sensor.register_binary_sensor(var, conf)

            hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
            cg.add(hub.register_component(var))
