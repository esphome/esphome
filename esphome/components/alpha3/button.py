import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import Alpha3, alpha3_ns

CONF_ALPHA3_ID = "alpha3_id"

Alpha3Button = alpha3_ns.class_("Alpha3Button", button.Button, cg.Component)
Alpha3ButtonAction = alpha3_ns.enum("Alpha3ButtonAction")

ACTION_START = Alpha3ButtonAction.ACTION_START
ACTION_STOP = Alpha3ButtonAction.ACTION_STOP
ACTION_SETPOINT_UP = Alpha3ButtonAction.ACTION_SETPOINT_UP
ACTION_SETPOINT_DOWN = Alpha3ButtonAction.ACTION_SETPOINT_DOWN

CONF_START_PUMP = "start_pump"
CONF_STOP_PUMP = "stop_pump"
CONF_SETPOINT_UP = "setpoint_up"
CONF_SETPOINT_DOWN = "setpoint_down"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_START_PUMP): button.button_schema(
            Alpha3Button,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_STOP_PUMP): button.button_schema(
            Alpha3Button,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_SETPOINT_UP): button.button_schema(
            Alpha3Button,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_SETPOINT_DOWN): button.button_schema(
            Alpha3Button,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALPHA3_ID])

    if start_config := config.get(CONF_START_PUMP):
        btn = await button.new_button(start_config)
        await cg.register_component(btn, start_config)
        cg.add(btn.set_parent(parent))
        cg.add(btn.set_action(ACTION_START))

    if stop_config := config.get(CONF_STOP_PUMP):
        btn = await button.new_button(stop_config)
        await cg.register_component(btn, stop_config)
        cg.add(btn.set_parent(parent))
        cg.add(btn.set_action(ACTION_STOP))

    if up_config := config.get(CONF_SETPOINT_UP):
        btn = await button.new_button(up_config)
        await cg.register_component(btn, up_config)
        cg.add(btn.set_parent(parent))
        cg.add(btn.set_action(ACTION_SETPOINT_UP))

    if down_config := config.get(CONF_SETPOINT_DOWN):
        btn = await button.new_button(down_config)
        await cg.register_component(btn, down_config)
        cg.add(btn.set_parent(parent))
        cg.add(btn.set_action(ACTION_SETPOINT_DOWN))
