import re
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_DIMENSIONS,
    CONF_TRIGGER_ID,
    CONF_ON_VALUE,
    CONF_COMMAND,
    CONF_NUMBER,
    CONF_FORMAT,
    CONF_LAMBDA,
)
from esphome.components import lcd_base
from esphome.automation import maybe_simple_id
from esphome.components.select import Select
from esphome.components.number import Number
from esphome.components.switch import Switch

CODEOWNERS = ["@numo68"]

lcd_menu_ns = cg.esphome_ns.namespace("lcd_menu")

CONF_DISPLAY_ID = "display_id"

CONF_ACTIVE = "active"
CONF_LABEL = "label"
CONF_MENU = "menu"
CONF_BACK = "back"
CONF_TEXT = "text"
CONF_SELECT = "select"
CONF_SWITCH = "switch"
CONF_ON_TEXT = "on_text"
CONF_OFF_TEXT = "off_text"
CONF_IMMEDIATE_EDIT = "immediate_edit"
CONF_MARK_SELECTED = "mark_selected"
CONF_MARK_EDITING = "mark_editing"
CONF_MARK_SUBMENU = "mark_submenu"
CONF_MARK_BACK = "mark_back"
CONF_ROOT_ITEM_ID = "root_item_id"
CONF_ON_ENTER = "on_enter"
CONF_ON_LEAVE = "on_leave"

LCDMenuComponent = lcd_menu_ns.class_("LCDMenuComponent", cg.Component)

MenuItem = lcd_menu_ns.class_("MenuItem")
MenuItemConstPtr = MenuItem.operator("ptr").operator("const")

UpAction = lcd_menu_ns.class_("UpAction", automation.Action)
DownAction = lcd_menu_ns.class_("DownAction", automation.Action)
EnterAction = lcd_menu_ns.class_("EnterAction", automation.Action)
ShowAction = lcd_menu_ns.class_("ShowAction", automation.Action)
HideAction = lcd_menu_ns.class_("HideAction", automation.Action)
ShowMainAction = lcd_menu_ns.class_("ShowMainAction", automation.Action)

IsActiveCondition = lcd_menu_ns.class_("IsActiveCondition", automation.Condition)

MULTI_CONF = True

MenuItemType = lcd_menu_ns.enum("MenuItemType")

MENU_ITEM_TYPES = {
    CONF_LABEL: MenuItemType.MENU_ITEM_LABEL,
    CONF_MENU: MenuItemType.MENU_ITEM_MENU,
    CONF_BACK: MenuItemType.MENU_ITEM_BACK,
    CONF_SELECT: MenuItemType.MENU_ITEM_SELECT,
    CONF_NUMBER: MenuItemType.MENU_ITEM_NUMBER,
    CONF_SWITCH: MenuItemType.MENU_ITEM_SWITCH,
    CONF_COMMAND: MenuItemType.MENU_ITEM_COMMAND,
}

LCDMenuOnEnterTrigger = lcd_menu_ns.class_("LCDMenuOnEnterTrigger", automation.Trigger)

LCDMenuOnLeaveTrigger = lcd_menu_ns.class_("LCDMenuOnLeaveTrigger", automation.Trigger)

LCDMenuOnValueTrigger = lcd_menu_ns.class_("LCDMenuOnValueTrigger", automation.Trigger)


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 64:
        raise cv.Invalid("LCD display can't have more than 64 columns")
    if value[0] < 12:
        raise cv.Invalid(
            "LCD display can't have less than 12 columns to be usable with the menu"
        )
    if value[1] > 4:
        raise cv.Invalid("LCD display can't have more than 4 rows")
    return value


def validate_menu_item(config):
    if config[CONF_TYPE] != CONF_MENU and CONF_MENU in config:
        raise cv.Invalid(
            f"{CONF_TYPE} has to be '{CONF_MENU}' if the {CONF_MENU} is present"
        )
    if config[CONF_TYPE] == CONF_MENU and CONF_MENU not in config:
        raise cv.Invalid(
            f"{CONF_MENU} has to be present if {CONF_TYPE} is '{CONF_MENU}'"
        )

    if config[CONF_TYPE] != CONF_SELECT:
        if CONF_SELECT in config:
            raise cv.Invalid(
                f"{CONF_TYPE} has to be '{CONF_SELECT}' if the {CONF_SELECT} is present"
            )

    if config[CONF_TYPE] == CONF_SELECT:
        if CONF_SELECT not in config:
            raise cv.Invalid(
                f"{CONF_SELECT} has to be present if {CONF_TYPE} is '{CONF_SELECT}'"
            )

    if config[CONF_TYPE] != CONF_NUMBER:
        if CONF_NUMBER in config:
            raise cv.Invalid(
                f"{CONF_TYPE} has to be '{CONF_NUMBER}' if the {CONF_NUMBER} is present"
            )
    if config[CONF_TYPE] == CONF_NUMBER:
        if CONF_NUMBER not in config:
            raise cv.Invalid(
                f"{CONF_NUMBER} has to be present if {CONF_TYPE} is '{CONF_NUMBER}'"
            )
        if re.search(r"^%([+-])*(\d+)*(\.\d+)*[fg]$", config[CONF_FORMAT]) is None:
            raise cv.Invalid(
                f"{CONF_FORMAT}: has to specify a printf-like format string specifying exactly one f or g type conversion, '{config[CONF_FORMAT]}' provided"
            )

    if config[CONF_TYPE] != CONF_SWITCH:
        if CONF_SWITCH in config:
            raise cv.Invalid(
                f"{CONF_TYPE} has to be '{CONF_SWITCH}' if the {CONF_SWITCH} is present"
            )

    if config[CONF_TYPE] == CONF_SWITCH:
        if CONF_SWITCH not in config:
            raise cv.Invalid(
                f"{CONF_SWITCH} has to be present if {CONF_TYPE} is '{CONF_SWITCH}'"
            )

    return config


# Use a simple indirection to circumvent the recursion limitation
def menu_item_schema(value):
    return MENU_ITEM_SCHEMA(value)


MENU_ITEM_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(MenuItem),
            cv.Optional(CONF_TYPE, default="label"): cv.enum(
                MENU_ITEM_TYPES, lower=True
            ),
            cv.Optional(CONF_TEXT): cv.string,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_ON_ENTER): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        LCDMenuOnEnterTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_LEAVE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        LCDMenuOnLeaveTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        LCDMenuOnValueTrigger
                    ),
                }
            ),
            cv.Optional(CONF_MENU): cv.All(
                cv.ensure_list(menu_item_schema), cv.Length(min=1)
            ),
            cv.Optional(CONF_SELECT): cv.use_id(Select),
            cv.Optional(CONF_IMMEDIATE_EDIT, default=False): cv.boolean,
            cv.Optional(CONF_NUMBER): cv.use_id(Number),
            cv.Optional(CONF_FORMAT, default="%.1f"): cv.string_strict,
            cv.Optional(CONF_SWITCH): cv.use_id(Switch),
            cv.Optional(CONF_ON_TEXT, default="On"): cv.string_strict,
            cv.Optional(CONF_OFF_TEXT, default="Off"): cv.string_strict,
        }
    ),
    validate_menu_item,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LCDMenuComponent),
        cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(lcd_base.LCDDisplay),
        cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,
        cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
        cv.Optional(CONF_MARK_SELECTED, default=0x3E): cv.uint8_t,
        cv.Optional(CONF_MARK_EDITING, default=0x2A): cv.uint8_t,
        cv.Optional(CONF_MARK_SUBMENU, default=0x7E): cv.uint8_t,
        cv.Optional(CONF_MARK_BACK, default=0x5E): cv.uint8_t,
        cv.GenerateID(CONF_ROOT_ITEM_ID): cv.declare_id(MenuItem),
        cv.Optional(CONF_ON_ENTER): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LCDMenuOnEnterTrigger),
            }
        ),
        cv.Optional(CONF_ON_LEAVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LCDMenuOnLeaveTrigger),
            }
        ),
        cv.Required(CONF_MENU): cv.All(
            cv.ensure_list(MENU_ITEM_SCHEMA), cv.Length(min=1)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

MENU_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(LCDMenuComponent),
    }
)


@automation.register_action("lcd_menu.up", UpAction, MENU_ACTION_SCHEMA)
async def menu_up_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("lcd_menu.down", DownAction, MENU_ACTION_SCHEMA)
async def menu_down_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("lcd_menu.enter", EnterAction, MENU_ACTION_SCHEMA)
async def menu_enter_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("lcd_menu.show", ShowAction, MENU_ACTION_SCHEMA)
async def menu_show_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("lcd_menu.hide", HideAction, MENU_ACTION_SCHEMA)
async def menu_hide_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("lcd_menu.show_main", ShowMainAction, MENU_ACTION_SCHEMA)
async def menu_show_main_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "lcd_menu.is_active",
    IsActiveCondition,
    automation.maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(LCDMenuComponent),
        }
    ),
)
async def lcd_menu_is_active_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


async def menu_item_to_code(menu, config, parent):
    item = cg.new_Pvariable(config[CONF_ID], MenuItem(config[CONF_TYPE]))
    cg.add(parent.add_item(item))
    if CONF_TEXT in config:
        cg.add(item.set_text(config[CONF_TEXT]))
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(MenuItemConstPtr, "it")], return_type=cg.std_string
        )
        cg.add(item.set_writer(template_))
    if CONF_MENU in config:
        for c in config[CONF_MENU]:
            await menu_item_to_code(menu, c, item)
    if config[CONF_TYPE] == CONF_SELECT:
        var = await cg.get_variable(config[CONF_SELECT])
        cg.add(item.set_select_variable(var))
        cg.add(item.set_immediate_edit(config[CONF_IMMEDIATE_EDIT]))
    if config[CONF_TYPE] == CONF_NUMBER:
        var = await cg.get_variable(config[CONF_NUMBER])
        cg.add(item.set_number_variable(var))
        cg.add(item.set_format(config[CONF_FORMAT]))
    if config[CONF_TYPE] == CONF_SWITCH:
        var = await cg.get_variable(config[CONF_SWITCH])
        cg.add(item.set_switch_variable(var))
        cg.add(item.set_on_text(config[CONF_ON_TEXT]))
        cg.add(item.set_off_text(config[CONF_OFF_TEXT]))
        cg.add(item.set_immediate_edit(config[CONF_IMMEDIATE_EDIT]))
    for conf in config.get(CONF_ON_ENTER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_LEAVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    disp = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))
    cg.add(var.set_dimensions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]))
    cg.add(var.set_active(config[CONF_ACTIVE]))
    cg.add(var.set_mark_selected(config[CONF_MARK_SELECTED]))
    cg.add(var.set_mark_editing(config[CONF_MARK_EDITING]))
    cg.add(var.set_mark_submenu(config[CONF_MARK_SUBMENU]))
    cg.add(var.set_mark_back(config[CONF_MARK_BACK]))
    root_item = cg.new_Pvariable(config[CONF_ROOT_ITEM_ID])
    cg.add(var.set_root_item(root_item))
    for c in config[CONF_MENU]:
        await menu_item_to_code(var, c, root_item)
    for conf in config.get(CONF_ON_ENTER, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], root_item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
    for conf in config.get(CONF_ON_LEAVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], root_item)
        await automation.build_automation(trigger, [(MenuItemConstPtr, "it")], conf)
