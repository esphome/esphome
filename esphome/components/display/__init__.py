import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, automation
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_PAGE_ID,
    CONF_ROTATION,
    CONF_FROM,
    CONF_TO,
    CONF_TRIGGER_ID,
)
from esphome.core import coroutine, coroutine_with_priority

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
DisplayOnPageChangeTrigger = display_ns.class_("DisplayOnPageChangeTrigger")

CONF_ON_PAGE_CHANGE = "on_page_change"

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

FULL_DISPLAY_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Optional(CONF_PAGES): cv.All(
            cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(DisplayPage),
                    cv.Required(CONF_LAMBDA): cv.lambda_,
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
    }
)


@coroutine
def setup_display_core_(var, config):
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))
    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            lambda_ = yield cg.process_lambda(
                conf[CONF_LAMBDA], [(DisplayBufferRef, "it")], return_type=cg.void
            )
            page = cg.new_Pvariable(conf[CONF_ID], lambda_)
            pages.append(page)
        cg.add(var.set_pages(pages))
    for conf in config.get(CONF_ON_PAGE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_FROM in conf:
            page = yield cg.get_variable(conf[CONF_FROM])
            cg.add(trigger.set_from(page))
        if CONF_TO in conf:
            page = yield cg.get_variable(conf[CONF_TO])
            cg.add(trigger.set_to(page))
        yield automation.build_automation(
            trigger, [(DisplayPagePtr, "from"), (DisplayPagePtr, "to")], conf
        )


@coroutine
def register_display(var, config):
    yield setup_display_core_(var, config)


@automation.register_action(
    "display.page.show",
    DisplayPageShowAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayPage)),
        }
    ),
)
def display_page_show_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    if isinstance(config[CONF_ID], core.Lambda):
        template_ = yield cg.templatable(config[CONF_ID], args, DisplayPagePtr)
        cg.add(var.set_page(template_))
    else:
        paren = yield cg.get_variable(config[CONF_ID])
        cg.add(var.set_page(paren))
    yield var


@automation.register_action(
    "display.page.show_next",
    DisplayPageShowNextAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
def display_page_show_next_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "display.page.show_previous",
    DisplayPageShowPrevAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
def display_page_show_previous_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


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
def display_is_displaying_page_to_code(config, condition_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    page = yield cg.get_variable(config[CONF_PAGE_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)
    cg.add(var.set_page(page))

    yield var


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(display_ns.using)
