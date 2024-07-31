import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor

from .. import CONF_OPENTHERM_ID, OpenthermHub, opentherm_ns, schema, generate

OpenthermSensor = opentherm_ns.class_("OpenthermSensor", sensor.Sensor, cg.Component)

DEPENDENCIES = ["opentherm", "sensor"]

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenthermHub),
    })
    .extend({
        cv.Optional(key): sensor.sensor_schema(
            OpenthermSensor.template(generate.get_type(entity))
            if generate.get_type(entity) is not None else
            OpenthermSensor,
            unit_of_measurement=entity["unit_of_measurement"] if "unit_of_measurement" in entity else sensor._UNDEF,
            icon=entity["icon"] if "icon" in entity else sensor._UNDEF,
            accuracy_decimals=entity["accuracy_decimals"] if "accuracy_decimals" in entity else sensor._UNDEF,
            device_class=entity["device_class"] if "device_class" in entity else sensor._UNDEF,
            state_class=entity["state_class"] if "state_class" in entity else sensor._UNDEF,
        )
        .extend(generate.opentherm_schema(entity))
        .extend(cv.COMPONENT_SCHEMA)
        for key, entity in schema.SENSORS.items()
    })
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    for key, conf in config.items():
        if isinstance(conf, dict):
            var = generate.create_opentherm_component(conf)

            await cg.register_component(var, conf)
            await sensor.register_sensor(var, conf)

            hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
            cg.add(hub.register_component(var))
