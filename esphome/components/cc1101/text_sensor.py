import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, ENTITY_CATEGORY_DIAGNOSTIC

from . import (
    CONF_CC1101_ID,
    CONF_MODULATION_TYPE,
    CONF_RX_ATTENUATION,
    CONF_TUNER,
    CC1101Component,
    ns,
)

CC1101TextSensor = ns.class_(
    "CC1101TextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONF_CHIP_ID = "chip_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
        cv.Optional(CONF_RX_ATTENUATION): text_sensor.text_sensor_schema(
            CC1101TextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CHIP_ID): text_sensor.text_sensor_schema(
            CC1101TextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_TUNER): cv.Schema(
            {
                cv.Optional(
                    CONF_MODULATION_TYPE.replace("_type", "")
                ): text_sensor.text_sensor_schema(
                    CC1101TextSensor,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
                cv.Optional(CONF_FREQUENCY): text_sensor.text_sensor_schema(
                    CC1101TextSensor,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                ),
            }
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    if CONF_RX_ATTENUATION in config:
        conf = config[CONF_RX_ATTENUATION]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_type(cg.RawExpression(f"{CC1101TextSensor}::RX_ATTENUATION")))

    if CONF_CHIP_ID in config:
        conf = config[CONF_CHIP_ID]
        var = await text_sensor.new_text_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_type(cg.RawExpression(f"{CC1101TextSensor}::CHIP_ID")))

    if CONF_TUNER in config:
        tuner_config = config[CONF_TUNER]

        # Mapping "modulation" to MODULATION_TYPE
        if "modulation" in tuner_config:
            conf = tuner_config["modulation"]
            var = await text_sensor.new_text_sensor(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101TextSensor}::MODULATION_TYPE"))
            )

        if CONF_FREQUENCY in tuner_config:
            conf = tuner_config[CONF_FREQUENCY]
            var = await text_sensor.new_text_sensor(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(cg.RawExpression(f"{CC1101TextSensor}::FREQUENCY")))
