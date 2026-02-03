import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_FREQUENCY, ENTITY_CATEGORY_DIAGNOSTIC
from . import CC1101Component, CONF_RX_ATTENUATION, CONF_MODULATION_TYPE, ns

CC1101TextSensor = ns.class_("CC1101TextSensor", text_sensor.TextSensor, cg.PollingComponent)

CONF_CC1101_ID = "cc1101_id"

TYPES = [
    CONF_RX_ATTENUATION,
    CONF_MODULATION_TYPE,
    CONF_FREQUENCY,
]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
}).extend(cv.polling_component_schema("60s"))

for type in TYPES:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): text_sensor.text_sensor_schema(
            CC1101TextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    })

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_text_sensor.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = await text_sensor.new_text_sensor(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101TextSensor, type.upper())))
