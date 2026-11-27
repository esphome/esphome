from esphome import automation, codegen as cg
from esphome.automation import ApplyAction, Trigger
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PAGES, CONF_TIME, CONF_TRIGGER_ID
from esphome.core import ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType, Expression, TemplateArgsType

from ..defines import (
    CONF_ANIMATION,
    CONF_PAGE,
    CONF_PAGE_WRAP,
    CONF_SKIP,
    LV_ANIM,
    literal,
)
from ..lv_validation import lv_bool, lv_milliseconds
from ..lvcode import (
    EVENT_ARG,
    LambdaContext,
    context_action,
    lv_add,
    lv_global,
    lvgl_parent,
    lvgl_static,
)
from ..schemas import LVGL_SCHEMA
from ..types import lv_page_t
from . import Widget, WidgetType, add_widgets, get_widgets, set_obj_properties

CONF_ON_LOAD = "on_load"
CONF_ON_UNLOAD = "on_unload"

PAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SKIP, default=False): lv_bool,
        cv.Optional(CONF_ON_LOAD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger.template()),
            }
        ),
        cv.Optional(CONF_ON_UNLOAD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger.template()),
            }
        ),
    }
)


class PageType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_PAGE,
            lv_page_t,
            (),
            PAGE_SCHEMA,
            modify_schema={},
        )

    async def to_code(self, w: Widget, config: dict):
        return []


SHOW_SCHEMA = LVGL_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATION, default="NONE"): LV_ANIM.one_of,
        cv.Optional(CONF_TIME, default="50ms"): lv_milliseconds,
    }
)

page_spec = PageType()


async def _show_page(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
    method: str,
    *page: Expression,
) -> MockObj:
    """Call ``method`` on the LVGL component with ``page`` and the animation settings."""
    async with LambdaContext(args, where=action_id) as context:
        animation = await LV_ANIM.process(config[CONF_ANIMATION])
        time = await lv_milliseconds.process(config[CONF_TIME])
        lv_add(getattr(await lvgl_parent(config), method)(*page, animation, time))
    return await context_action(context, action_id, template_arg, args)


@automation.register_action(
    "lvgl.page.next",
    ApplyAction,
    SHOW_SCHEMA,
    synchronous=True,
)
async def page_next_to_code(config, action_id, template_arg, args):
    return await _show_page(config, action_id, template_arg, args, "show_next_page")


automation.register_apply_condition(
    "lvgl.page.is_showing",
    cv.maybe_simple_value(
        cv.Schema({cv.Required(CONF_ID): cv.use_id(lv_page_t)}),
        key=CONF_ID,
    ),
    "is_showing()",
)


@automation.register_action(
    "lvgl.page.previous",
    ApplyAction,
    SHOW_SCHEMA,
    synchronous=True,
)
async def page_previous_to_code(config, action_id, template_arg, args):
    return await _show_page(config, action_id, template_arg, args, "show_prev_page")


@automation.register_action(
    "lvgl.page.show",
    ApplyAction,
    cv.maybe_simple_value(
        SHOW_SCHEMA.extend(
            {
                cv.Required(CONF_ID): cv.use_id(lv_page_t),
            }
        ),
        key=CONF_ID,
    ),
    synchronous=True,
)
async def page_show_to_code(config, action_id, template_arg, args):
    widget = await lv_global(config[CONF_ID])
    return await _show_page(
        config, action_id, template_arg, args, "show_page", widget.index
    )


async def add_pages(lv_component, config):
    lv_add(lv_component.set_page_wrap(config[CONF_PAGE_WRAP]))
    for pconf in config.get(CONF_PAGES, ()):
        id = pconf[CONF_ID]
        skip = pconf[CONF_SKIP]
        var = cg.new_Pvariable(id, skip)
        page = Widget.create(id, var, page_spec, pconf)
        lv_add(lv_component.add_page(var))
        # Set outer config first
        await set_obj_properties(page, config)
        await set_obj_properties(page, pconf)
        await add_widgets(page, pconf)


async def generate_page_triggers(config):
    for pconf in config.get(CONF_PAGES, ()):
        page = (await get_widgets(pconf))[0]
        for ev in (CONF_ON_LOAD, CONF_ON_UNLOAD):
            for loaded in pconf.get(ev, ()):
                trigger = cg.new_Pvariable(loaded[CONF_TRIGGER_ID])
                await automation.build_automation(trigger, [], loaded)
                async with LambdaContext(EVENT_ARG, where=id) as context:
                    lv_add(trigger.trigger())
                lv_add(
                    lvgl_static.add_event_cb(
                        page.obj,
                        await context.get_lambda(),
                        literal(f"LV_EVENT_SCREEN_{ev[3:].upper()}_START"),
                    )
                )
