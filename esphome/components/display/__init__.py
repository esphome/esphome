import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, automation
from esphome.automation import maybe_simple_id
from esphome.components import sensor, text_sensor
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_PAGE_ID,
    CONF_ROTATION,
    CONF_FROM,
    CONF_TO,
    CONF_TRIGGER_ID,
)
from esphome.core import coroutine_with_priority

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

Widget = display_ns.class_("Widget")
WidgetRef = Widget.operator("ref")
WidgetContainer = display_ns.class_("WidgetContainer", Widget)
Horizontal = display_ns.class_("Horizontal", Widget)
Vertical = display_ns.class_("Vertical", Widget)
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

CONF_X = "x"
CONF_Y = "y"
CONF_WIDTH = "width"
CONF_HEIGHT = "height"


def WidgetSchema(x):
    return WIDGET_SCHEMA(x)


BASE_WIDGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_X, default=0): cv.int_range(min=0, max=2000),
        cv.Optional(CONF_Y, default=0): cv.int_range(min=0, max=2000),
        cv.Optional(CONF_WIDTH): cv.Any(cv.int_range(min=0, max=2000), cv.percentage),
        cv.Optional(CONF_HEIGHT): cv.Any(cv.int_range(min=0, max=2000), cv.percentage),
    },
)

HORIZONTAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Horizontal),
        cv.Required("horizontal"): cv.All(
            cv.ensure_list(WidgetSchema),
            cv.Length(min=1),
        ),
    },
).extend(BASE_WIDGET_SCHEMA)

VERTICAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Vertical),
        cv.Required("vertical"): cv.All(
            cv.ensure_list(WidgetSchema),
            cv.Length(min=1),
        ),
    },
).extend(BASE_WIDGET_SCHEMA)


def use_font_id(value):
    from esphome.components import font

    return cv.use_id(font.Font)(value)


TEXT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Text.template()),
        cv.Required("text"): cv.templatable(cv.string),
        cv.Required("font"): use_font_id,
        cv.Exclusive("sensor", "sensor"): cv.use_id(sensor.Sensor),
        cv.Exclusive("text_sensor", "sensor"): cv.use_id(text_sensor.TextSensor),
    },
)

WIDGET_SCHEMA = cv.Any(
    HORIZONTAL_SCHEMA,
    VERTICAL_SCHEMA,
    TEXT_SCHEMA,
)

FULL_DISPLAY_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Optional(CONF_PAGES): cv.All(
            cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(DisplayPage),
                    cv.GenerateID(CONF_WIDGET_CONTAINER_ID): cv.declare_id(
                        WidgetContainer
                    ),
                    cv.Optional(CONF_WIDGETS): cv.ensure_list(WidgetSchema),
                    cv.Optional(CONF_LAMBDA): cv.lambda_,
                }
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


async def setup_widget(conf) -> WidgetRef:
    var = cg.new_Pvariable(conf[CONF_ID])
    if "horizontal" in conf or "vertical" in conf:
        children = []
        for w in conf.get("horizontal", []) + conf.get("vertical", []):
            w = await setup_widget(w)
            children.append(w)
        cg.add(var.set_children(children))
    elif "text" in conf:
        text = await cg.templatable(conf["text"], (), cg.std_string)
        for key in ("sensor", "text_sensor"):
            if key in conf:
                sensor = await cg.get_variable(conf[key])
                cg.add(var.set_sensor(sensor))
        cg.add(var.set_text(text))
        font = await cg.get_variable(conf["font"])
        cg.add(var.set_font(font))
    return var


async def setup_widgets(widget_container_id, widgets):
    var = cg.new_Pvariable(widget_container_id)
    children = []
    for conf in widgets:
        w = await setup_widget(conf)
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
