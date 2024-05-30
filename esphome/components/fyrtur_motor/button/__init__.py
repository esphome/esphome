import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_STOP
from .. import CONF_FYRTUR_MOTOR_ID, FyrturMotorComponent, fyrtur_motor_ns

MoveUpButton = fyrtur_motor_ns.class_("MoveUpButton", button.Button)
MoveDownButton = fyrtur_motor_ns.class_("MoveDownButton", button.Button)
StopButton = fyrtur_motor_ns.class_("StopButton", button.Button)
ResetMaxLengthButton = fyrtur_motor_ns.class_("ResetMaxLengthButton", button.Button)
SetMaxLengthButton = fyrtur_motor_ns.class_("SetMaxLengthButton", button.Button)
ToggleRollDirectionButton = fyrtur_motor_ns.class_(
    "ToggleRollDirectionButton", button.Button
)

CONF_MOVE_UP = "move_up"
CONF_MOVE_DOWN = "move_down"
CONF_SET_MAX_LENGTH = "set_max_length"
CONF_RESET_MAX_LENGTH = "reset_max_length"
CONF_TOGGLE_ROLL_DIRECTION = "toggle_roll_direction"

ICON_UP = "mdi:arrow-up-bold"
ICON_DOWN = "mdi:arrow-down-bold"
ICON_STOP = "mdi:stop"
ICON_RESET_MAX_LENGTH = "mdi:arrow-collapse-vertical"
ICON_SET_MAX_LENGTH = "mdi:arrow-collapse-down"
ICON_REVERSE = "mdi:arrow-oscillating"

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
    cv.Required(CONF_SET_MAX_LENGTH): button.button_schema(
        SetMaxLengthButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_SET_MAX_LENGTH,
    ),
    cv.Optional(CONF_RESET_MAX_LENGTH): button.button_schema(
        ResetMaxLengthButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESET_MAX_LENGTH,
    ),
    cv.Optional(CONF_TOGGLE_ROLL_DIRECTION): button.button_schema(
        ToggleRollDirectionButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_REVERSE,
    ),
}


async def to_code(config):
    fyrtur_motor_component = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    if move_up_config := config.get(CONF_MOVE_UP):
        b = await button.new_button(move_up_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_move_up_button(b))
    if move_down_config := config.get(CONF_MOVE_DOWN):
        b = await button.new_button(move_down_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_move_down_button(b))
    if stop_config := config.get(CONF_STOP):
        b = await button.new_button(stop_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_stop_button(b))
    if set_max_length_config := config.get(CONF_SET_MAX_LENGTH):
        b = await button.new_button(set_max_length_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_set_max_length_button(b))
    if reset_max_length_config := config.get(CONF_RESET_MAX_LENGTH):
        b = await button.new_button(reset_max_length_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_reset_max_length_button(b))
    if toggle_roll_direction_config := config.get(CONF_TOGGLE_ROLL_DIRECTION):
        b = await button.new_button(toggle_roll_direction_config)
        await cg.register_parented(b, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_toggle_roll_direction_button(b))
