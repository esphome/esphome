from esphome import automation, core
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_FROM,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGE_ID,
    CONF_PAGES,
    CONF_ROTATION,
    CONF_TO,
    CONF_TRIGGER_ID,
)
from esphome.core import coroutine_with_priority

IS_PLATFORM_COMPONENT = True

display_ns = cg.esphome_ns.namespace("display")
Display = display_ns.class_("Display", cg.PollingComponent)
DisplayBuffer = display_ns.class_("DisplayBuffer", Display)
DisplayPage = display_ns.class_("DisplayPage")
DisplayPagePtr = DisplayPage.operator("ptr")
DisplayRef = Display.operator("ref")
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

CONF_ON_PAGE_CHANGE = "on_page_change"
CONF_SHOW_TEST_CARD = "show_test_card"
CONF_UNSPECIFIED = "unspecified"

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


def validate_auto_clear(value):
    if value == CONF_UNSPECIFIED:
        return value
    return cv.boolean(value)


BASIC_DISPLAY_SCHEMA = cv.Schema(
    {
        cv.Exclusive(CONF_LAMBDA, CONF_LAMBDA): cv.lambda_,
    }
).extend(cv.polling_component_schema("1s"))

FULL_DISPLAY_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Exclusive(CONF_PAGES, CONF_LAMBDA): cv.All(
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
        cv.Optional(
            CONF_AUTO_CLEAR_ENABLED, default=CONF_UNSPECIFIED
        ): validate_auto_clear,
        cv.Optional(CONF_SHOW_TEST_CARD): cv.boolean,
    }
)


async def setup_display_core_(var, config):
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))

    if auto_clear := config.get(CONF_AUTO_CLEAR_ENABLED):
        # Default to true if pages or lambda is specified. Ideally this would be done during validation, but
        # the possible schemas are too complex to do this easily.
        if auto_clear == CONF_UNSPECIFIED:
            auto_clear = CONF_LAMBDA in config or CONF_PAGES in config
        cg.add(var.set_auto_clear(auto_clear))

    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            lambda_ = await cg.process_lambda(
                conf[CONF_LAMBDA], [(DisplayRef, "it")], return_type=cg.void
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
    if config.get(CONF_SHOW_TEST_CARD):
        cg.add(var.show_test_card())


async def register_display(var, config):
    await cg.register_component(var, config)
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
            cv.Required(CONF_ID): cv.templatable(cv.use_id(Display)),
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
            cv.Required(CONF_ID): cv.templatable(cv.use_id(Display)),
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
            cv.GenerateID(CONF_ID): cv.use_id(Display),
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
    cg.add_define("USE_DISPLAY")
