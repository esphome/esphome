from dataclasses import dataclass

from esphome import automation, core
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.const import (
    BYTE_ORDER_BIG,
    CONF_BYTE_ORDER,
    CONF_DRAW_ROUNDING,
    KEY_METADATA,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_DIMENSIONS,
    CONF_FROM,
    CONF_HEIGHT,
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGE_ID,
    CONF_PAGES,
    CONF_ROTATION,
    CONF_TO,
    CONF_TRIGGER_ID,
    CONF_UPDATE_INTERVAL,
    CONF_WIDTH,
    SCHEDULER_DONT_RUN,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.final_validate import full_config

DOMAIN = "display"
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
    value = value.removesuffix("°")
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


def _validate_test_card(config):
    if (
        config.get(CONF_SHOW_TEST_CARD, False)
        and config.get(CONF_UPDATE_INTERVAL, False) == SCHEDULER_DONT_RUN
    ):
        raise cv.Invalid(
            f"`{CONF_SHOW_TEST_CARD}: True` cannot be used with `{CONF_UPDATE_INTERVAL}: never` because this combination will not show a test_card."
        )
    return config


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
FULL_DISPLAY_SCHEMA.add_extra(_validate_test_card)


async def setup_display_core_(var, config):
    if rotation := config.get(CONF_ROTATION, 0):
        # Default initialised value for rotation is 0
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[rotation]))

    if (auto_clear := config.get(CONF_AUTO_CLEAR_ENABLED)) is not None:
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


# Storage of display metadata in a central location, accessible via the id


@dataclass(frozen=True)
class DisplayMetaData:
    width: int = 0
    height: int = 0
    has_hardware_rotation: bool = False
    byte_order: str = BYTE_ORDER_BIG
    has_writer: bool = False
    rotation: int = 0
    draw_rounding: int = 0


def _get_metadata_list() -> list[tuple]:
    """Get the raw metadata list. Each entry is (id, DisplayMetaData)."""
    return CORE.data.setdefault(DOMAIN, {}).setdefault(KEY_METADATA, [])


def get_all_display_metadata() -> dict[str, DisplayMetaData]:
    """Get all display metadata as a dict keyed by resolved ID strings.

    Must not be called before IDs have been finalised.
    """
    entries = _get_metadata_list()
    assert all(id_.id is not None for id_, _ in entries), (
        "get_all_display_metadata called before display IDs have been resolved"
    )
    return {id_.id: meta for id_, meta in entries}


def get_display_metadata(display_id: ID) -> DisplayMetaData:
    """Get display metadata by ID object

    Must not be called before IDs have been finalised.
    """
    for id_, meta in _get_metadata_list():
        if id_ is display_id:
            return meta
        assert id_.id is not None, (
            "get_display_metadata called before display IDs have been resolved"
        )
        if id_.id == display_id.id:
            return meta
    # No metadata found, display driver may not yet support it.
    # Read the raw config to populate the returned data
    global_config = full_config.get()
    path = global_config.get_path_for_id(display_id)[:-1]
    disp_config = global_config.get_config_for_path(path)
    dimensions = disp_config.get(CONF_DIMENSIONS, (0, 0))
    if isinstance(dimensions, dict):
        dimensions = (dimensions.get(CONF_WIDTH, 0), dimensions.get(CONF_HEIGHT, 0))
    elif not isinstance(dimensions, tuple) or len(dimensions) != 2:
        dimensions = (0, 0)

    meta = DisplayMetaData(
        width=dimensions[0],
        height=dimensions[1],
        has_hardware_rotation=False,
        byte_order=disp_config.get(CONF_BYTE_ORDER, cv.UNDEFINED),
        has_writer=disp_config.get(CONF_AUTO_CLEAR_ENABLED) is True
        or disp_config.get(CONF_PAGES) is not None
        or disp_config.get(CONF_LAMBDA) is not None
        or disp_config.get(CONF_SHOW_TEST_CARD) is True,
        rotation=disp_config.get(CONF_ROTATION, 0),
        draw_rounding=disp_config.get(CONF_DRAW_ROUNDING, 0),
    )
    _get_metadata_list().append((display_id, meta))
    return meta


def add_metadata(
    id: ID,
    width: int,
    height: int,
    has_hardware_rotation: bool = False,
    byte_order: str = BYTE_ORDER_BIG,
    has_writer: bool = False,
    rotation: int = 0,
    draw_rounding: int = 0,
):
    entries = _get_metadata_list()
    assert not any(existing_id is id for existing_id, _ in entries), (
        f"Duplicate display metadata for ID {id}"
    )
    entries.append(
        (
            id,
            DisplayMetaData(
                width=width,
                height=height,
                has_hardware_rotation=has_hardware_rotation,
                byte_order=byte_order,
                has_writer=has_writer,
                rotation=rotation,
                draw_rounding=draw_rounding,
            ),
        )
    )


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
    synchronous=True,
)
async def display_page_show_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    if isinstance(config[CONF_ID], core.Lambda):
        template_ = await cg.templatable(config[CONF_ID], args, DisplayPagePtr)
        cg.add(var.set_page(template_))
    else:
        paren = await cg.get_variable(config[CONF_ID])
        template_ = await cg.templatable(paren, args, DisplayPagePtr)
        cg.add(var.set_page(template_))
    return var


@automation.register_action(
    "display.page.show_next",
    DisplayPageShowNextAction,
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.templatable(cv.use_id(Display)),
        }
    ),
    synchronous=True,
)
async def display_page_show_next_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "display.page.show_previous",
    DisplayPageShowPrevAction,
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.templatable(cv.use_id(Display)),
        }
    ),
    synchronous=True,
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


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(display_ns.using)
    cg.add_define("USE_DISPLAY")
    if CORE.is_esp32:
        # Re-enable ESP-IDF's LCD driver (excluded by default to save compile time)
        from esphome.components.esp32 import include_builtin_idf_component

        include_builtin_idf_component("esp_lcd")
