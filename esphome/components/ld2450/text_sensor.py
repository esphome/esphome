import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_DIRECTION
from . import CONF_LD2450_ID, LD2450Component, NUM_TARGETS

DEPENDENCIES = ["ld2450"]

CONF_DIRECTION = "direction"
CONF_POSITION = "position"


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    **{
        cv.Optional(f"target{n}"): {
            cv.Optional(CONF_DIRECTION): text_sensor.text_sensor_schema(
                entity_category="DIAGNOSTIC",
                icon=ICON_DIRECTION,
            ),
            cv.Optional(CONF_POSITION): text_sensor.text_sensor_schema(
                entity_category="DIAGNOSTIC", icon=ICON_DIRECTION
            ),
        }
        for n in range(NUM_TARGETS)
    },
}


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    for n in range(NUM_TARGETS):
        if target_conf := config.get(f"target{n}"):
            if direction_config := target_conf.get(CONF_DIRECTION):
                sens = await text_sensor.new_text_sensor(direction_config)
                cg.add(ld2450_component.get_target(n).set_direction_text_sensor(sens))
            if position_config := target_conf.get(CONF_POSITION):
                sens = await text_sensor.new_text_sensor(position_config)
                cg.add(ld2450_component.get_target(n).set_position_text_sensor(sens))
