import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_MODE, ENTITY_CATEGORY_CONFIG, ICON_MOTION_SENSOR
from esphome.cpp_generator import LambdaExpression

from .. import AMG8833, CONF_AMG8833_ID, amg8833_ns

SetterSelect = amg8833_ns.class_("SetterSelect", select.Select)

const_std_string_ref = cg.global_ns.class_("const std::string &")

CONF_FPS = "fps"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833),
    cv.Optional(CONF_FPS): select.select_schema(
        SetterSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:animation",
    ),
    cv.Optional(CONF_MODE): select.select_schema(
        SetterSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    ),
}


async def to_code(config):
    amg8833_component = await cg.get_variable(config[CONF_AMG8833_ID])
    if fps_config := config.get(CONF_FPS):
        s = await select.new_select(fps_config, options=["FPS_10", "FPS_1"])
        cg.add(amg8833_component.set_fps_select(s))
        cg.add(
            s.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->select_fps(value);",
                    [(const_std_string_ref, "value")],
                )
            )
        )
    if interrupt_mode_config := config.get(CONF_MODE):
        s = await select.new_select(
            interrupt_mode_config, options=["MOTION", "PRESENCE"]
        )
        cg.add(amg8833_component.set_mode_select(s))
        cg.add(
            s.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->select_mode(value);",
                    [(const_std_string_ref, "value")],
                )
            )
        )
