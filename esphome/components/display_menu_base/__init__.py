import re

from esphome import automation, core
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.number import Number
from esphome.components.select import Select
from esphome.components.switch import Switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_COMMAND,
    CONF_CUSTOM,
    CONF_FORMAT,
    CONF_ID,
    CONF_ITEMS,
    CONF_MODE,
    CONF_NUMBER,
    CONF_ON_VALUE,
    CONF_TEXT,
    CONF_TRIGGER_ID,
    CONF_TYPE,
)

CODEOWNERS = ["@numo68"]

display_menu_base_ns = cg.esphome_ns.namespace("display_menu_base")


CONF_ROTARY = "rotary"
CONF_JOYSTICK = "joystick"
CONF_LABEL = "label"
CONF_MENU = "menu"
CONF_BACK = "back"
CONF_SELECT = "select"
CONF_SWITCH = "switch"
CONF_ON_TEXT = "on_text"
CONF_OFF_TEXT = "off_text"
CONF_VALUE_LAMBDA = "value_lambda"
CONF_IMMEDIATE_EDIT = "immediate_edit"
CONF_ROOT_ITEM_ID = "root_item_id"
CONF_ON_ENTER = "on_enter"
CONF_ON_LEAVE = "on_leave"
CONF_ON_NEXT = "on_next"
CONF_ON_PREV = "on_prev"

DisplayMenuComponent = display_menu_base_ns.class_("DisplayMenuComponent", cg.Component)

MenuItem = display_menu_base_ns.class_("MenuItem")
MenuItemConstPtr = MenuItem.operator("ptr").operator("const")
MenuItemMenu = display_menu_base_ns.class_("MenuItemMenu")
MenuItemSelect = display_menu_base_ns.class_("MenuItemSelect")
MenuItemNumber = display_menu_base_ns.class_("MenuItemNumber")
MenuItemSwitch = display_menu_base_ns.class_("MenuItemSwitch")
MenuItemCommand = display_menu_base_ns.class_("MenuItemCommand")
MenuItemCustom = display_menu_base_ns.class_("MenuItemCustom")

UpAction = display_menu_base_ns.class_("UpAction", automation.Action)
DownAction = display_menu_base_ns.class_("DownAction", automation.Action)
LeftAction = display_menu_base_ns.class_("LeftAction", automation.Action)
RightAction = display_menu_base_ns.class_("RightAction", automation.Action)
EnterAction = display_menu_base_ns.class_("EnterAction", automation.Action)
ShowAction = display_menu_base_ns.class_("ShowAction", automation.Action)
HideAction = display_menu_base_ns.class_("HideAction", automation.Action)
ShowMainAction = display_menu_base_ns.class_("ShowMainAction", automation.Action)

IsActiveCondition = display_menu_base_ns.class_(
    "IsActiveCondition", automation.Condition
)

MULTI_CONF = True

MenuItemType = display_menu_base_ns.enum("MenuItemType")

MENU_ITEM_TYPES = {
    CONF_LABEL: MenuItemType.MENU_ITEM_LABEL,
    CONF_MENU: MenuItemType.MENU_ITEM_MENU,
    CONF_BACK: MenuItemType.MENU_ITEM_BACK,
    CONF_SELECT: MenuItemType.MENU_ITEM_SELECT,
    CONF_NUMBER: MenuItemType.MENU_ITEM_NUMBER,
    CONF_SWITCH: MenuItemType.MENU_ITEM_SWITCH,
    CONF_COMMAND: MenuItemType.MENU_ITEM_COMMAND,
    CONF_CUSTOM: MenuItemType.MENU_ITEM_CUSTOM,
}

MENU_ITEMS_WITH_SPECIALIZED_CLASSES = (
    CONF_MENU,
    CONF_SELECT,
    CONF_NUMBER,
    CONF_SWITCH,
    CONF_COMMAND,
    CONF_CUSTOM,
)

MenuMode = display_menu_base_ns.enum("MenuMode")

MENU_MODES = {
    CONF_ROTARY: MenuMode.MENU_MODE_ROTARY,
    CONF_JOYSTICK: MenuMode.MENU_MODE_JOYSTICK,
}

DisplayMenuOnEnterTrigger = display_menu_base_ns.class_(
    "DisplayMenuOnEnterTrigger", automation.Trigger
)

DisplayMenuOnLeaveTrigger = display_menu_base_ns.class_(
    "DisplayMenuOnLeaveTrigger", automation.Trigger
)

DisplayMenuOnValueTrigger = display_menu_base_ns.class_(
    "DisplayMenuOnValueTrigger", automation.Trigger
)

DisplayMenuOnNextTrigger = display_menu_base_ns.class_(
    "DisplayMenuOnNextTrigger", automation.Trigger
)

DisplayMenuOnPrevTrigger = display_menu_base_ns.class_(
    "DisplayMenuOnPrevTrigger", automation.Trigger
)


def validate_format(format):
    if re.search(r"^%([+-])*(\d+)*(\.\d+)*[fg]$", format) is None:
        raise cv.Invalid(
            f"{CONF_FORMAT}: has to specify a printf-like format string specifying exactly one f or g type conversion, '{format}' provided"
        )

    return format


# Use a simple indirection to circumvent the recursion limitation
def menu_item_schema(value):
    return MENU_ITEM_SCHEMA(value)


MENU_ITEM_COMMON_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEXT): cv.templatable(cv.string),
    }
)

MENU_ITEM_ENTER_LEAVE_SCHEMA = MENU_ITEM_COMMON_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_ENTER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnEnterTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_LEAVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnLeaveTrigger
                ),
            }
        ),
    }
)

MENU_ITEM_VALUE_SCHEMA = MENU_ITEM_COMMON_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnValueTrigger
                ),
            }
        ),
    }
)

MENU_ITEM_ENTER_LEAVE_VALUE_SCHEMA = MENU_ITEM_ENTER_LEAVE_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnValueTrigger
                ),
            }
        ),
    }
)

MENU_ITEM_SCHEMA = cv.typed_schema(
    {
        CONF_LABEL: MENU_ITEM_COMMON_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItem),
            }
        ),
        CONF_BACK: MENU_ITEM_COMMON_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItem),
            }
        ),
        CONF_MENU: MENU_ITEM_ENTER_LEAVE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemMenu),
                cv.Required(CONF_ITEMS): cv.All(
                    cv.ensure_list(menu_item_schema), cv.Length(min=1)
                ),
            }
        ),
        CONF_SELECT: MENU_ITEM_ENTER_LEAVE_VALUE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemSelect),
                cv.Required(CONF_SELECT): cv.use_id(Select),
                cv.Optional(CONF_IMMEDIATE_EDIT, default=False): cv.boolean,
                cv.Optional(CONF_VALUE_LAMBDA): cv.returning_lambda,
            }
        ),
        CONF_NUMBER: MENU_ITEM_ENTER_LEAVE_VALUE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemNumber),
                cv.Required(CONF_NUMBER): cv.use_id(Number),
                cv.Optional(CONF_IMMEDIATE_EDIT, default=False): cv.boolean,
                cv.Optional(CONF_FORMAT, default="%.1f"): cv.All(
                    cv.string_strict,
                    validate_format,
                ),
                cv.Optional(CONF_VALUE_LAMBDA): cv.returning_lambda,
            }
        ),
        CONF_SWITCH: MENU_ITEM_ENTER_LEAVE_VALUE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemSwitch),
                cv.Required(CONF_SWITCH): cv.use_id(Switch),
                cv.Optional(CONF_IMMEDIATE_EDIT, default=False): cv.boolean,
                cv.Optional(CONF_ON_TEXT, default="On"): cv.string_strict,
                cv.Optional(CONF_OFF_TEXT, default="Off"): cv.string_strict,
                cv.Optional(CONF_VALUE_LAMBDA): cv.returning_lambda,
            }
        ),
        CONF_COMMAND: MENU_ITEM_VALUE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemCommand),
            }
        ),
        CONF_CUSTOM: MENU_ITEM_ENTER_LEAVE_VALUE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(MenuItemCustom),
                cv.Optional(CONF_IMMEDIATE_EDIT, default=False): cv.boolean,
                cv.Optional(CONF_VALUE_LAMBDA): cv.returning_lambda,
                cv.Optional(CONF_ON_NEXT): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            DisplayMenuOnNextTrigger
                        ),
                    }
                ),
                cv.Optional(CONF_ON_PREV): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            DisplayMenuOnPrevTrigger
                        ),
                    }
                ),
            }
        ),
    },
    default_type="label",
    lower=True,
)

DISPLAY_MENU_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
        cv.GenerateID(CONF_ROOT_ITEM_ID): cv.declare_id(MenuItemMenu),
        cv.Optional(CONF_MODE, default=CONF_ROTARY): cv.enum(MENU_MODES),
        cv.Optional(CONF_ON_ENTER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnEnterTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_LEAVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayMenuOnLeaveTrigger
                ),
            }
        ),
        cv.Required(CONF_ITEMS): cv.All(
            cv.ensure_list(MENU_ITEM_SCHEMA), cv.Length(min=1)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

MENU_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(DisplayMenuComponent),
    }
)


@automation.register_action("display_menu.up", UpAction, MENU_ACTION_SCHEMA)
async def menu_up_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.down", DownAction, MENU_ACTION_SCHEMA)
async def menu_down_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.left", LeftAction, MENU_ACTION_SCHEMA)
async def menu_left_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.right", RightAction, MENU_ACTION_SCHEMA)
async def menu_right_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.enter", EnterAction, MENU_ACTION_SCHEMA)
async def menu_enter_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.show", ShowAction, MENU_ACTION_SCHEMA)
async def menu_show_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("display_menu.hide", HideAction, MENU_ACTION_SCHEMA)
async def menu_hide_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "display_menu.show_main", ShowMainAction, MENU_ACTION_SCHEMA
)
async def menu_show_main_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "display_menu.is_active",
    IsActiveCondition,
    automation.maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(DisplayMenuComponent),
        }
    ),
)
async def display_menu_is_active_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


async def menu_item_to_code(menu, config, parent):
    if config[CONF_TYPE] in MENU_ITEMS_WITH_SPECIALIZED_CLASSES:
        item = cg.new_Pvariable(config[CONF_ID])
    else:
        item = cg.new_Pvariable(config[CONF_ID], MENU_ITEM_TYPES[config[CONF_TYPE]])
    cg.add(parent.add_item(item))
    if CONF_TEXT in config:
        if isinstance(config[CONF_TEXT], core.Lambda):
            template_ = await cg.templatable(
                config[CONF_TEXT], [(MenuItemConstPtr, "it")], cg.std_string
            )
            cg.add(item.set_text(template_))
        else:
            cg.add(item.set_text(config[CONF_TEXT]))
    if CONF_VALUE_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_VALUE_LAMBDA],
            [(MenuItemConstPtr, "it")],
            return_type=cg.std_string,
        )
        cg.add(item.set_value_lambda(template_))
    if CONF_ITEMS in config:
        for c in config[CONF_ITEMS]:
            await menu_item_to_code(menu, c, item)
    if CONF_IMMEDIATE_EDIT in config:
        cg.add(item.set_immediate_edit(config[CONF_IMMEDIATE_EDIT]))
    if config[CONF_TYPE] == CONF_SELECT:
        var = await cg.get_variable(config[CONF_SELECT])
        cg.add(item.set_select_variable(var))
    if config[CONF_TYPE] == CONF_NUMBER:
        var = await cg.get_variable(config[CONF_NUMBER])
        cg.add(item.set_number_variable(var))
        cg.add(item.set_format(config[CONF_FORMAT]))
    if config[CONF_TYPE] == CONF_SWITCH:
        var = await cg.get_variable(config[CONF_SWITCH])
        cg.add(item.set_switch_variable(var))
        cg.add(item.set_on_text(config[CONF_ON_TEXT]))
        cg.add(item.set_off_text(config[CONF_OFF_TEXT]))
    for conf in config.get(CONF_ON_ENTER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_LEAVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_NEXT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_PREV, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)


async def display_menu_to_code(menu, config):
    cg.add(menu.set_active(config[CONF_ACTIVE]))
    root_item = cg.new_Pvariable(config[CONF_ROOT_ITEM_ID])
    cg.add(menu.set_root_item(root_item))
    cg.add(menu.set_mode(config[CONF_MODE]))
    for c in config[CONF_ITEMS]:
        await menu_item_to_code(menu, c, root_item)
    for conf in config.get(CONF_ON_ENTER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], root_item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_LEAVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], root_item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
