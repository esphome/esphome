import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_LAMBDA
from esphome.components import binary_sensor, display, font
from esphome.helpers import color

CODEOWNERS = ["@numo68"]
AUTO_LOAD = ["binary_sensor"]

touch_gui_ns = cg.esphome_ns.namespace("touch_gui")

CONF_DISPLAY_ID = "display_id"
CONF_X = "x"
CONF_Y = "y"
CONF_TOUCHED = "touched"
CONF_BUTTON_COLORS = "button_colors"
CONF_BACKGROUND = "background"
CONF_ACTIVE_BACKGROUND = "active_background"
CONF_FOREGROUND = "foreground"
CONF_ACTIVE_FOREGROUND = "active_foreground"
CONF_BORDER = "border"
CONF_BUTTON_FONT = "button_font"
CONF_BUTTON = "button"
CONF_ON_UPDATE = "on_update"

TouchGUIComponent = touch_gui_ns.class_("TouchGUIComponent", cg.PollingComponent)
TouchGUITouchAction = touch_gui_ns.class_("TouchGUITouchAction", automation.Action)
TouchGUIActivateButtonAction = touch_gui_ns.class_(
    "TouchGUIActivateButtonAction", automation.Action
)
TouchGuiButton = touch_gui_ns.class_(
    "TouchGUIButton", binary_sensor.BinarySensor, cg.Component
)
TouchGuiUpdateTrigger = touch_gui_ns.class_(
    "TouchGuiUpdateTrigger", automation.Trigger.template()
)

MULTI_CONF = True

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TouchGUIComponent),
        cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.DisplayBuffer),
        cv.Optional(CONF_BUTTON_COLORS): cv.Schema(
            {
                cv.Optional(CONF_BACKGROUND): cv.use_id(color),
                cv.Optional(CONF_ACTIVE_BACKGROUND): cv.use_id(color),
                cv.Optional(CONF_FOREGROUND): cv.use_id(color),
                cv.Optional(CONF_ACTIVE_FOREGROUND): cv.use_id(color),
                cv.Optional(CONF_BORDER): cv.use_id(color),
            }
        ),
        cv.Optional(CONF_BUTTON_FONT): cv.use_id(font),
        cv.Optional(CONF_LAMBDA): cv.lambda_,
        cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TouchGuiUpdateTrigger),
            }
        ),
    }
).extend(cv.polling_component_schema("50ms"))


@automation.register_action(
    "touch_gui.touch",
    TouchGUITouchAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(TouchGUIComponent),
            cv.Optional(CONF_X, default=0): cv.templatable(cv.positive_int),
            cv.Optional(CONF_Y, default=0): cv.templatable(cv.positive_int),
            cv.Optional(CONF_TOUCHED, default=True): cv.templatable(cv.boolean),
        }
    ),
)
def touch_gui_touch_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)

    gui = yield cg.get_variable(config[CONF_ID])
    cg.add(var.set_gui(gui))

    if isinstance(config[CONF_X], core.Lambda):
        template_ = yield cg.templatable(config[CONF_X], args, int)
        cg.add(var.set_x(template_))
    else:
        cg.add(var.set_x(config[CONF_X]))

    if isinstance(config[CONF_Y], core.Lambda):
        template_ = yield cg.templatable(config[CONF_Y], args, int)
        cg.add(var.set_y(template_))
    else:
        cg.add(var.set_y(config[CONF_Y]))

    if isinstance(config[CONF_TOUCHED], core.Lambda):
        template_ = yield cg.templatable(config[CONF_TOUCHED], args, bool)
        cg.add(var.set_touched(template_))
    else:
        cg.add(var.set_touched(config[CONF_TOUCHED]))

    yield var


@automation.register_action(
    "touch_gui.activate_button",
    TouchGUIActivateButtonAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(TouchGUIComponent),
            cv.Required(CONF_BUTTON): cv.templatable(cv.use_id(TouchGuiButton)),
        },
        key=CONF_BUTTON,
    ),
)
def touch_gui_activate_button_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)

    gui = yield cg.get_variable(config[CONF_ID])
    cg.add(var.set_gui(gui))

    if isinstance(config[CONF_BUTTON], core.Lambda):
        template_ = yield cg.templatable(
            config[CONF_BUTTON], args, TouchGuiButton.operator("ptr")
        )
        cg.add(var.set_button(template_))
    else:
        btn = yield cg.get_variable(config[CONF_BUTTON])
        cg.add(var.set_button(btn))

    yield var


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    disp = yield cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(disp))
    if CONF_BUTTON_COLORS in config:
        colors = config[CONF_BUTTON_COLORS]
        if CONF_BACKGROUND in colors:
            col = yield cg.get_variable(colors[CONF_BACKGROUND])
            cg.add(var.set_button_background_color(col))
        if CONF_ACTIVE_BACKGROUND in colors:
            col = yield cg.get_variable(colors[CONF_ACTIVE_BACKGROUND])
            cg.add(var.set_button_active_background_color(col))
        if CONF_FOREGROUND in colors:
            col = yield cg.get_variable(colors[CONF_FOREGROUND])
            cg.add(var.set_button_foreground_color(col))
        if CONF_ACTIVE_FOREGROUND in colors:
            col = yield cg.get_variable(colors[CONF_ACTIVE_FOREGROUND])
            cg.add(var.set_button_active_foreground_color(col))
        if CONF_BORDER in colors:
            col = yield cg.get_variable(colors[CONF_BORDER])
            cg.add(var.set_button_border_color(col))
    if CONF_BUTTON_FONT in config:
        fnt = yield cg.get_variable(config[CONF_BUTTON_FONT])
        cg.add(var.set_button_font(fnt))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA],
            [(TouchGuiButton.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
    for conf in config.get(CONF_ON_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)
