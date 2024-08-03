from esphome import automation, codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PAGES, CONF_TIME

from .defines import (
    CONF_ANIMATION,
    CONF_LVGL_ID,
    CONF_PAGE,
    CONF_PAGE_WRAP,
    CONF_SKIP,
    LV_ANIM,
)
from .lv_validation import lv_bool, lv_milliseconds
from .lvcode import LVGL_COMP_ARG, LambdaContext, add_line_marks, lv_add, lvgl_comp
from .schemas import LVGL_SCHEMA
from .types import LvglAction, lv_page_t
from .widget import Widget, WidgetType, add_widgets, set_obj_properties


class PageType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_PAGE,
            lv_page_t,
            (),
            {
                cv.Optional(CONF_SKIP, default=False): lv_bool,
            },
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


@automation.register_action(
    "lvgl.page.next",
    LvglAction,
    SHOW_SCHEMA,
)
async def page_next_to_code(config, action_id, template_arg, args):
    animation = await LV_ANIM.process(config[CONF_ANIMATION])
    time = await lv_milliseconds.process(config[CONF_TIME])
    async with LambdaContext(LVGL_COMP_ARG) as context:
        add_line_marks(action_id)
        lv_add(lvgl_comp.show_next_page(animation, time))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var


@automation.register_action(
    "lvgl.page.previous",
    LvglAction,
    SHOW_SCHEMA,
)
async def page_previous_to_code(config, action_id, template_arg, args):
    animation = await LV_ANIM.process(config[CONF_ANIMATION])
    time = await lv_milliseconds.process(config[CONF_TIME])
    async with LambdaContext(LVGL_COMP_ARG) as context:
        add_line_marks(action_id)
        lv_add(lvgl_comp.show_prev_page(animation, time))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var


@automation.register_action(
    "lvgl.page.show",
    LvglAction,
    cv.maybe_simple_value(
        SHOW_SCHEMA.extend(
            {
                cv.Required(CONF_ID): cv.use_id(lv_page_t),
            }
        ),
        key=CONF_ID,
    ),
)
async def page_show_to_code(config, action_id, template_arg, args):
    widget = await cg.get_variable(config[CONF_ID])
    animation = await LV_ANIM.process(config[CONF_ANIMATION])
    time = await lv_milliseconds.process(config[CONF_TIME])
    async with LambdaContext(LVGL_COMP_ARG) as context:
        add_line_marks(action_id)
        lv_add(lvgl_comp.show_page(widget.index, animation, time))
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var


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
