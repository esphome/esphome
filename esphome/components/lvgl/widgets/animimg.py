from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID

from ..automation import action_to_code
from ..defines import CONF_AUTO_START, CONF_MAIN, CONF_REPEAT_COUNT, CONF_SRC
from ..helpers import lvgl_components_required
from ..lv_validation import lv_image, lv_milliseconds
from ..lvcode import lv
from ..types import LvType, ObjUpdateAction, void_ptr
from . import Widget, WidgetType, get_widgets
from .img import CONF_IMAGE
from .label import CONF_LABEL

CONF_ANIMIMG = "animimg"
CONF_SRC_LIST_ID = "src_list_id"


def lv_repeat_count(value):
    if isinstance(value, str) and value.lower() in ("forever", "infinite"):
        value = 0xFFFF
    return cv.int_range(min=0, max=0xFFFF)(value)


ANIMIMG_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPEAT_COUNT, default="forever"): lv_repeat_count,
        cv.Optional(CONF_AUTO_START, default=True): cv.boolean,
    }
)
ANIMIMG_SCHEMA = ANIMIMG_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_DURATION): lv_milliseconds,
        cv.Required(CONF_SRC): cv.ensure_list(lv_image),
        cv.GenerateID(CONF_SRC_LIST_ID): cv.declare_id(void_ptr),
    }
)

ANIMIMG_MODIFY_SCHEMA = ANIMIMG_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_DURATION): lv_milliseconds,
    }
)

lv_animimg_t = LvType("lv_animimg_t")


class AnimimgType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ANIMIMG,
            lv_animimg_t,
            (CONF_MAIN,),
            ANIMIMG_SCHEMA,
            ANIMIMG_MODIFY_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        lvgl_components_required.add(CONF_IMAGE)
        lvgl_components_required.add(CONF_ANIMIMG)
        if CONF_SRC in config:
            for x in config[CONF_SRC]:
                await cg.get_variable(x)
            srcs = [await lv_image.process(x) for x in config[CONF_SRC]]
            src_id = cg.static_const_array(config[CONF_SRC_LIST_ID], srcs)
            count = len(config[CONF_SRC])
            lv.animimg_set_src(w.obj, src_id, count)
        lv.animimg_set_repeat_count(w.obj, config[CONF_REPEAT_COUNT])
        lv.animimg_set_duration(w.obj, config[CONF_DURATION])
        if config.get(CONF_AUTO_START):
            lv.animimg_start(w.obj)

    def get_uses(self):
        return "img", CONF_IMAGE, CONF_LABEL


animimg_spec = AnimimgType()


@automation.register_action(
    "lvgl.animimg.start",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_start(config, action_id, template_arg, args):
    widget = await get_widgets(config)

    async def do_start(w: Widget):
        lv.animimg_start(w.obj)

    return await action_to_code(widget, do_start, action_id, template_arg, args)


@automation.register_action(
    "lvgl.animimg.stop",
    ObjUpdateAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.use_id(lv_animimg_t),
        },
        key=CONF_ID,
    ),
)
async def animimg_stop(config, action_id, template_arg, args):
    widget = await get_widgets(config)

    async def do_stop(w: Widget):
        lv.animimg_stop(w.obj)

    return await action_to_code(widget, do_stop, action_id, template_arg, args)
