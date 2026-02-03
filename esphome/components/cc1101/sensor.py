import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID, CONF_RSSI, CONF_LQI, UNIT_DECIBEL_MILLIWATT, UNIT_EMPTY,
    DEVICE_CLASS_SIGNAL_STRENGTH, STATE_CLASS_MEASUREMENT, ENTITY_CATEGORY_DIAGNOSTIC
)
from . import CC1101Component, ns

CC1101Sensor = ns.class_("CC1101Sensor", sensor.Sensor, cg.Component)

CONF_CC1101_ID = "cc1101_id"

TYPES = {
    CONF_RSSI: {
        "unit": UNIT_DECIBEL_MILLIWATT,
        "device_class": DEVICE_CLASS_SIGNAL_STRENGTH,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 1,
    },
    CONF_LQI: {
        "unit": UNIT_EMPTY,
        "device_class": None,
        "state_class": STATE_CLASS_MEASUREMENT,
        "accuracy_decimals": 0,
    }
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
})

for type, data in TYPES.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): sensor.sensor_schema(
            CC1101Sensor,
            unit_of_measurement=data["unit"],
            accuracy_decimals=data["accuracy_decimals"],
            device_class=data.get("device_class"),
            state_class=data["state_class"],
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    })

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_sensor.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    for type, data in TYPES.items():
        if type in config:
            conf = config[type]
            var = await sensor.new_sensor(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101Sensor, type.upper())))
