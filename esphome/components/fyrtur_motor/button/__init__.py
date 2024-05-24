import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_STOP
from .. import CONF_FYRTUR_MOTOR_ID, FyrturMotorComponent, fyrtur_motor_ns

MoveUpButton = fyrtur_motor_ns.class_("MoveUpButton", button.Button)
MoveDownButton = fyrtur_motor_ns.class_("MoveDownButton", button.Button)
StopButton = fyrtur_motor_ns.class_("StopButton", button.Button)
GetStatusButton = fyrtur_motor_ns.class_("GetStatusButton", button.Button)

CONF_MOVE_UP = "move_up"
CONF_MOVE_DOWN = "move_down"
CONF_GET_STATUS = "get_status"

ICON_UP = "mdi:arrow-up-bold"
ICON_DOWN = "mdi:arrow-down-bold"
ICON_STOP = "mdi:stop"
ICON_SYNC = "mdi:sync"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
    cv.Required(CONF_MOVE_UP): button.button_schema(
        MoveUpButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_UP,
    ),
    cv.Required(CONF_MOVE_DOWN): button.button_schema(
        MoveDownButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_DOWN,
    ),
    cv.Required(CONF_STOP): button.button_schema(
        StopButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_STOP,
    ),
    cv.Optional(CONF_GET_STATUS): button.button_schema(
        GetStatusButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SYNC,
    ),
}


async def to_code(config):
    fyrtur_motor_component = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    if move_up_config := config.get(CONF_MOVE_UP):
        b = await button.new_button(move_up_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.move_up_button(b))
    if move_down_config := config.get(CONF_MOVE_DOWN):
        b = await button.new_button(move_down_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.move_down_button(b))
    if stop_config := config.get(CONF_STOP):
        b = await button.new_button(stop_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.stop_button(b))
    if get_status_config := config.get(CONF_GET_STATUS):
        b = await button.new_button(get_status_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.get_status_button(b))
