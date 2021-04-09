import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, display, font
from esphome.components.color import ColorStruct
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    CONF_PAGES,
    CONF_COLORS,
    CONF_LAMBDA,
)
from . import (
    touch_gui_ns,
    TouchGUIComponent,
    TouchGuiButton,
    CONF_BACKGROUND,
    CONF_ACTIVE_BACKGROUND,
    CONF_FOREGROUND,
    CONF_ACTIVE_FOREGROUND,
    CONF_BORDER,
)

CONF_TOUCH_GUI_ID = "touch_gui_id"
CONF_MOMENTARY = "momentary"
CONF_TOGGLE = "toggle"
CONF_RADIO = "radio"
CONF_AREA = "area"
CONF_X_MIN = "x_min"
CONF_X_MAX = "x_max"
CONF_Y_MIN = "y_min"
CONF_Y_MAX = "y_max"
CONF_FONT = "font"
CONF_TEXT = "text"
CONF_RADIO_GROUP = "radio_group"
CONF_INITIAL = "initial"
CONF_TOUCH_TIME = "touch_time"

DEPENDENCIES = ["touch_gui"]

TouchGUIButtonType = touch_gui_ns.enum("TouchGUIButtonType")

BUTTON_TYPES = {
    CONF_MOMENTARY: TouchGUIButtonType.TOUCH_GUI_BUTTON_TYPE_MOMENTARY,
    CONF_TOGGLE: TouchGUIButtonType.TOUCH_GUI_BUTTON_TYPE_TOGGLE,
    CONF_RADIO: TouchGUIButtonType.TOUCH_GUI_BUTTON_TYPE_RADIO,
    CONF_AREA: TouchGUIButtonType.TOUCH_GUI_BUTTON_TYPE_AREA,
}


def validate(conf):
    if conf[CONF_X_MAX] < conf[CONF_X_MIN] + 8:
        raise cv.Invalid(
            "x_max ({}) must be at least 8 more than x_min ({})".format(
                conf[CONF_X_MAX], conf[CONF_X_MIN]
            )
        )
    if conf[CONF_Y_MAX] < conf[CONF_Y_MIN] + 8:
        raise cv.Invalid(
            "y_max ({}) must be at least 8 more than y_min ({})".format(
                conf[CONF_Y_MAX], conf[CONF_Y_MIN]
            )
        )

    if conf[CONF_TYPE] == CONF_RADIO and CONF_RADIO_GROUP not in conf:
        raise cv.Invalid("radio_group has to be present for type: radio")

    if conf[CONF_TYPE] != CONF_RADIO and CONF_RADIO_GROUP in conf:
        raise cv.Invalid(
            "radio_group must not be present for type: {}".format(conf[CONF_TYPE])
        )

    if CONF_INITIAL in conf and conf[CONF_TYPE] not in [CONF_RADIO, CONF_TOGGLE]:
        raise cv.Invalid("initial is only allowed for types toggle and radio")

    return conf


CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TouchGuiButton),
            cv.GenerateID(CONF_TOUCH_GUI_ID): cv.use_id(TouchGUIComponent),
            cv.Required(CONF_TYPE): cv.enum(BUTTON_TYPES, lower=True),
            cv.Required(CONF_X_MIN): cv.positive_int,
            cv.Required(CONF_X_MAX): cv.positive_int,
            cv.Required(CONF_Y_MIN): cv.positive_int,
            cv.Required(CONF_Y_MAX): cv.positive_int,
            cv.Optional(CONF_RADIO_GROUP): cv.positive_not_null_int,
            cv.Optional(CONF_INITIAL): cv.boolean,
            cv.Optional(
                CONF_TOUCH_TIME, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PAGES): cv.All(
                cv.ensure_list(
                    cv.maybe_simple_value(
                        {
                            cv.GenerateID(CONF_ID): cv.use_id(display.DisplayPage),
                        },
                        key=CONF_ID,
                    ),
                ),
                cv.Length(min=1),
            ),
            cv.Optional(CONF_COLORS): cv.Schema(
                {
                    cv.Optional(CONF_BACKGROUND): cv.use_id(ColorStruct),
                    cv.Optional(CONF_ACTIVE_BACKGROUND): cv.use_id(ColorStruct),
                    cv.Optional(CONF_FOREGROUND): cv.use_id(ColorStruct),
                    cv.Optional(CONF_ACTIVE_FOREGROUND): cv.use_id(ColorStruct),
                    cv.Optional(CONF_BORDER): cv.use_id(ColorStruct),
                }
            ),
            cv.Optional(CONF_FONT): cv.use_id(font),
            cv.Optional(CONF_TEXT): cv.templatable(cv.string),
            cv.Optional(CONF_LAMBDA): cv.lambda_,
        }
    ),
    validate,
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)
    t = BUTTON_TYPES[config[CONF_TYPE]]
    cg.add(var.set_type(t))
    cg.add(
        var.set_area(
            config[CONF_X_MIN],
            config[CONF_X_MAX],
            config[CONF_Y_MIN],
            config[CONF_Y_MAX],
        )
    )
    hub = yield cg.get_variable(config[CONF_TOUCH_GUI_ID])
    cg.add(var.set_parent(hub))
    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            page = yield cg.get_variable(conf[CONF_ID])
            pages.append(page)
        cg.add(var.set_pages(pages))
    if CONF_COLORS in config:
        colors = config[CONF_COLORS]
        if CONF_BACKGROUND in colors:
            col = yield cg.get_variable(colors[CONF_BACKGROUND])
            cg.add(var.set_background_color(col))
        if CONF_ACTIVE_BACKGROUND in colors:
            col = yield cg.get_variable(colors[CONF_ACTIVE_BACKGROUND])
            cg.add(var.set_active_background_color(col))
        if CONF_FOREGROUND in colors:
            col = yield cg.get_variable(colors[CONF_FOREGROUND])
            cg.add(var.set_foreground_color(col))
        if CONF_ACTIVE_FOREGROUND in colors:
            col = yield cg.get_variable(colors[CONF_ACTIVE_FOREGROUND])
            cg.add(var.set_active_foreground_color(col))
        if CONF_BORDER in colors:
            col = yield cg.get_variable(colors[CONF_BORDER])
            cg.add(var.set_border_color(col))
    if CONF_FONT in config:
        fnt = yield cg.get_variable(config[CONF_FONT])
        cg.add(var.set_font(fnt))
    if CONF_RADIO_GROUP in config:
        cg.add(var.set_radio_group(config[CONF_RADIO_GROUP]))
    if CONF_INITIAL in config:
        cg.add(var.set_initial(config[CONF_INITIAL]))
    if CONF_TOUCH_TIME in config:
        cg.add(var.set_touch_time(config[CONF_TOUCH_TIME]))
    if CONF_TEXT in config:
        template_ = yield cg.templatable(config[CONF_TEXT], [], cg.std_string)
        cg.add(var.set_text(template_))
    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA],
            [(TouchGuiButton.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))

    cg.add(hub.register_button(var))
