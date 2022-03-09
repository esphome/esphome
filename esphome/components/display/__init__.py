import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_FORMAT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_PAGE_ID,
    CONF_ROTATION,
    CONF_SENSOR,
    CONF_FROM,
    CONF_TEXT_SENSOR,
    CONF_TO,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
)
from esphome.core import coroutine_with_priority
from esphome.util import Registry

IS_PLATFORM_COMPONENT = True

display_ns = cg.esphome_ns.namespace("display")
DisplayBuffer = display_ns.class_("DisplayBuffer")
DisplayPage = display_ns.class_("DisplayPage")
DisplayPagePtr = DisplayPage.operator("ptr")
DisplayBufferRef = DisplayBuffer.operator("ref")
DisplayPageShowAction = display_ns.class_("DisplayPageShowAction", automation.Action)
DisplayPageShowNextAction = display_ns.class_(
    "DisplayPageShowNextAction", automation.Action
)
DisplayPageShowPrevAction = display_ns.class_(
    "DisplayPageShowPrevAction", automation.Action
)
DisplayIsDisplayingPageCondition = display_ns.class_(
    "DisplayIsDisplayingPageCondition", automation.Condition
)
DisplayOnPageChangeTrigger = display_ns.class_(
    "DisplayOnPageChangeTrigger", automation.Trigger
)
TextAlign = display_ns.enum("TextAlign", is_class=True)

Widget = display_ns.class_("Widget")
WidgetRef = Widget.operator("ref")
WidgetContainer = display_ns.class_("WidgetContainer", Widget)
Text = display_ns.class_("Text", Widget)

CONF_ON_PAGE_CHANGE = "on_page_change"

CONF_WIDGETS = "widgets"

DISPLAY_ROTATIONS = {
    0: display_ns.DISPLAY_ROTATION_0_DEGREES,
    90: display_ns.DISPLAY_ROTATION_90_DEGREES,
    180: display_ns.DISPLAY_ROTATION_180_DEGREES,
    270: display_ns.DISPLAY_ROTATION_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith("°"):
        value = value[:-1]
    return cv.enum(DISPLAY_ROTATIONS, int=True)(value)


BASIC_DISPLAY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LAMBDA): cv.lambda_,
    }
)

CONF_WIDGET_CONTAINER_ID = "widget_container_id"

CONF_MINIMUM_SIZE = "minimum_size"
CONF_PREFERRED_SIZE = "preferred_size"
CONF_MAXIMUM_SIZE = "maximum_size"

DIMENSION_SOURCE = display_ns.namespace("Widget").enum("DimensionSource")

_DIMENSION_SOURCE = {
    "AUTO": DIMENSION_SOURCE.AUTO,
    "MINIMUM": DIMENSION_SOURCE.MINIMUM,
    "PREFERRED": DIMENSION_SOURCE.PREFERRED,
    "MAXIMUM": DIMENSION_SOURCE.MAXIMUM,
    "INFINITE": DIMENSION_SOURCE.INFINITE,
}


def size(value):
    try:
        return cv.enum(_DIMENSION_SOURCE, upper=True)(value)
    except cv.Invalid:
        pass
    try:
        value = int(value)
    except (TypeError, ValueError):
        raise cv.Invalid(
            'Width and height dimensions must be integers or "auto", "minimum", "preferred", "maximum", or "infinite"'
        )
    if value < 0:
        raise cv.Invalid("Width and height must be at least 0")
    return value


def dimensions(value):
    # Four possibilities:
    # "NNxNN"
    # [NN, NN]
    # "maximum"
    # ["maximum", NN]
    try:
        value = size(value)
        return [value, value]
    except cv.Invalid:
        pass
    if isinstance(value, list):
        if len(value) != 2:
            raise cv.Invalid(f"Dimensions must have a length of two, not {len(value)}")
        return size(value[0]), size(value[1])
    value = cv.string(value)
    match = re.match(r"\s*([0-9]+)\s*[xX]\s*([0-9]+)\s*", value)
    if not match:
        raise cv.Invalid(
            "Invalid value '{}' for dimensions. Only WIDTHxHEIGHT is allowed."
        )
    return dimensions([match.group(1), match.group(2)])


WIDGET_REGISTRY = Registry(
    {
        cv.Optional(CONF_MINIMUM_SIZE): dimensions,
        cv.Optional(CONF_PREFERRED_SIZE): dimensions,
        cv.Optional(CONF_MAXIMUM_SIZE): dimensions,
    },
)
validate_widget = cv.validate_registry_entry(
    "widget",
    WIDGET_REGISTRY,
)
register_widget = WIDGET_REGISTRY.register


async def build_widget(full_config):
    registry_entry, config = cg.extract_registry_entry_config(
        WIDGET_REGISTRY, full_config
    )
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    var = cg.new_Pvariable(type_id)
    if CONF_MINIMUM_SIZE in full_config:
        cg.add(var.set_minimum_size(*full_config[CONF_MINIMUM_SIZE]))
    if CONF_PREFERRED_SIZE in full_config:
        cg.add(var.set_preferred_size(*full_config[CONF_PREFERRED_SIZE]))
    if CONF_MAXIMUM_SIZE in full_config:
        cg.add(var.set_maximum_size(*full_config[CONF_MAXIMUM_SIZE]))
    await builder(var, config)
    return var


for class_ in ("Horizontal", "Vertical"):

    @register_widget(
        class_.lower(),
        display_ns.class_(class_, Widget),
        cv.All(
            cv.ensure_list(validate_widget),
            cv.Length(min=1),
        ),
    )
    async def box_widget(var, config):
        children = []
        for w in config:
            w = await build_widget(w)
            children.append(w)
        cg.add(var.set_children(children))


@register_widget(
    "button",
    display_ns.class_("Button", Widget),
    {
        cv.Required("content"): validate_widget,
    },
)
async def button_widget(var, config):
    w = await build_widget(config["content"])
    cg.add(var.set_child(w))


def use_font_id(value):
    from esphome.components import font

    return cv.use_id(font.Font)(value)


_ALIGN = {
    "TOP_LEFT": TextAlign.TOP_LEFT,
    "TOP_CENTER": TextAlign.TOP_CENTER,
    "TOP_RIGHT": TextAlign.TOP_RIGHT,
    "CENTER_LEFT": TextAlign.CENTER_LEFT,
    "CENTER": TextAlign.CENTER,
    "CENTER_RIGHT": TextAlign.CENTER_RIGHT,
    "BASELINE_LEFT": TextAlign.BASELINE_LEFT,
    "BASELINE_CENTER": TextAlign.BASELINE_CENTER,
    "BASELINE_RIGHT": TextAlign.BASELINE_RIGHT,
    "BOTTOM_LEFT": TextAlign.BOTTOM_LEFT,
    "BOTTOM_CENTER": TextAlign.BOTTOM_CENTER,
    "BOTTOM_RIGHT": TextAlign.BOTTOM_RIGHT,
}

CONF_TEXT = "text"
CONF_FONT = "font"
CONF_ALIGN = "align"


def validate_text(obj):
    if CONF_TEXT in obj:
        obj[CONF_FORMAT] = obj[CONF_TEXT]
    if CONF_SENSOR in obj and CONF_FORMAT not in obj:
        obj[CONF_FORMAT] = "%g"
    if CONF_TEXT_SENSOR in obj and CONF_FORMAT not in obj:
        obj[CONF_FORMAT] = "%s"
    if CONF_FORMAT not in obj:
        raise cv.Invalid("text, format, or a sensor must be specified")
    return obj


@register_widget(
    "text",
    Text.template(),
    cv.All(
        {
            cv.Exclusive(CONF_TEXT, "text"): cv.templatable(cv.string),
            cv.Exclusive(CONF_FORMAT, "text"): cv.templatable(cv.string),
            cv.Required(CONF_FONT): use_font_id,
            cv.Exclusive(CONF_SENSOR, "sensor"): cv.use_id(sensor.Sensor),
            cv.Exclusive(CONF_TEXT_SENSOR, "sensor"): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_ALIGN, default="top left"): cv.enum(
                _ALIGN, upper=True, space="_"
            ),
        },
        validate_text,
    ),
)
async def text_widget(var, conf):
    cg.add(var.set_textalign(conf[CONF_ALIGN].enum_value))
    text = await cg.templatable(conf[CONF_FORMAT], (), cg.std_string)
    for key in (CONF_SENSOR, CONF_TEXT_SENSOR):
        if key in conf:
            cg.add(var.set_sensor(await cg.get_variable(conf[key])))
    cg.add(var.set_text(text))
    font = await cg.get_variable(conf[CONF_FONT])
    cg.add(var.set_font(font))


FULL_DISPLAY_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Optional(CONF_PAGES): cv.All(
            cv.ensure_list(
                cv.All(
                    {
                        cv.GenerateID(): cv.declare_id(DisplayPage),
                        cv.GenerateID(CONF_WIDGET_CONTAINER_ID): cv.declare_id(
                            WidgetContainer
                        ),
                        cv.Exclusive(CONF_WIDGETS, "draw"): cv.ensure_list(
                            validate_widget
                        ),
                        cv.Exclusive(CONF_LAMBDA, "draw"): cv.lambda_,
                    },
                    cv.has_exactly_one_key(CONF_WIDGETS, CONF_LAMBDA),
                )
            ),
            cv.Length(min=1),
        ),
        cv.Optional(CONF_ON_PAGE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    DisplayOnPageChangeTrigger
                ),
                cv.Optional(CONF_FROM): cv.use_id(DisplayPage),
                cv.Optional(CONF_TO): cv.use_id(DisplayPage),
            }
        ),
        cv.Optional(CONF_AUTO_CLEAR_ENABLED, default=True): cv.boolean,
    }
)


async def setup_display_core_(var, config):
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))

    if CONF_AUTO_CLEAR_ENABLED in config:
        cg.add(var.set_auto_clear(config[CONF_AUTO_CLEAR_ENABLED]))

    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            if CONF_LAMBDA in conf:
                lambda_ = await cg.process_lambda(
                    conf[CONF_LAMBDA], [(DisplayBufferRef, "it")], return_type=cg.void
                )
            elif CONF_WIDGETS in conf:
                lambda_ = await setup_widgets(
                    conf[CONF_WIDGET_CONTAINER_ID], conf[CONF_WIDGETS]
                )
            page = cg.new_Pvariable(conf[CONF_ID], lambda_)
            pages.append(page)
        cg.add(var.set_pages(pages))
    for conf in config.get(CONF_ON_PAGE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_FROM in conf:
            page = await cg.get_variable(conf[CONF_FROM])
            cg.add(trigger.set_from(page))
        if CONF_TO in conf:
            page = await cg.get_variable(conf[CONF_TO])
            cg.add(trigger.set_to(page))
        await automation.build_automation(
            trigger, [(DisplayPagePtr, "from"), (DisplayPagePtr, "to")], conf
        )


async def setup_widgets(widget_container_id, widgets):
    var = cg.new_Pvariable(widget_container_id)
    children = []
    for conf in widgets:
        w = await build_widget(conf)
        children.append(w)
    cg.add(var.set_children(children))
    return cg.std_ns.bind(
        display_ns.namespace("WidgetContainer").draw_fullscreen.operator("addr"),
        var,
        cg.std_ns.namespace("placeholders").namespace("_1"),
    )


async def register_display(var, config):
    await setup_display_core_(var, config)


@automation.register_action(
    "display.page.show",
    DisplayPageShowAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayPage)),
        }
    ),
)
async def display_page_show_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    if isinstance(config[CONF_ID], core.Lambda):
        template_ = await cg.templatable(config[CONF_ID], args, DisplayPagePtr)
        cg.add(var.set_page(template_))
    else:
        paren = await cg.get_variable(config[CONF_ID])
        cg.add(var.set_page(paren))
    return var


@automation.register_action(
    "display.page.show_next",
    DisplayPageShowNextAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
async def display_page_show_next_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "display.page.show_previous",
    DisplayPageShowPrevAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
async def display_page_show_previous_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition(
    "display.is_displaying_page",
    DisplayIsDisplayingPageCondition,
    cv.maybe_simple_value(
        {
            cv.GenerateID(CONF_ID): cv.use_id(DisplayBuffer),
            cv.Required(CONF_PAGE_ID): cv.use_id(DisplayPage),
        },
        key=CONF_PAGE_ID,
    ),
)
async def display_is_displaying_page_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    page = await cg.get_variable(config[CONF_PAGE_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)
    cg.add(var.set_page(page))

    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(display_ns.using)
