from esphome import automation, codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME

from .codegen import action_to_code, add_widgets, set_obj_properties
from .defines import CONF_ANIMATION, CONF_LVGL_ID, CONF_PAGE, CONF_SKIP, LV_ANIM
from .lv_validation import lv_bool, lv_milliseconds
from .schemas import LVGL_SCHEMA
from .types import ObjUpdateAction, lv_page_t
from .widget import Widget, WidgetType, get_widget


class PageType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_PAGE,
            {
                cv.Optional(CONF_SKIP, default=False): lv_bool,
            },
        )

    @property
    def w_type(self):
        return lv_page_t

    async def to_code(self, w: Widget, config: dict):
        return []

    async def page_to_code(self, config, pconf, index):
        init = []
        id = pconf[CONF_ID]
        var = cg.new_Pvariable(id)
        page = Widget.create(id, var, page_spec, config, f"{var}->page")
        init.append(f"{page.var}->index = {index}")
        init.append(f"{page.obj} = lv_obj_create(nullptr)")
        skip = pconf[CONF_SKIP]
        init.append(f"{var}->skip = {skip}")
        # Set outer config first
        init.extend(await set_obj_properties(page, config))
        init.extend(await set_obj_properties(page, pconf))
        init.extend(await add_widgets(page, pconf))
        return var, init


SHOW_SCHEMA = LVGL_SCHEMA.extend(
    {
        cv.Optional(CONF_ANIMATION, default="NONE"): LV_ANIM.one_of,
        cv.Optional(CONF_TIME, default="50ms"): lv_milliseconds,
    }
)


page_spec = PageType()


@automation.register_action(
    "lvgl.page.next",
    ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_next_to_code(config, action_id, template_arg, args):
    lv_comp = await get_widget(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = await lv_milliseconds.process(config[CONF_TIME])
    init = [f"{lv_comp.obj}->show_next_page(false, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.previous",
    ObjUpdateAction,
    SHOW_SCHEMA,
)
async def page_previous_to_code(config, action_id, template_arg, args):
    lv_comp = await get_widget(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp.obj}->show_next_page(true, {animation}, {time})"]
    return await action_to_code(init, action_id, lv_comp, template_arg, args)


@automation.register_action(
    "lvgl.page.show",
    ObjUpdateAction,
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
    widget = await get_widget(config[CONF_ID])
    lv_comp = await cg.get_variable(config[CONF_LVGL_ID])
    animation = config[CONF_ANIMATION]
    time = config[CONF_TIME].total_milliseconds
    init = [f"{lv_comp}->show_page({widget.var}->index, {animation}, {time})"]
    return await action_to_code(init, action_id, widget, template_arg, args)
