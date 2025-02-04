from esphome import automation
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_ON_VALUE, CONF_TRIGGER_ID

from .defines import (
    CONF_ALIGN,
    CONF_ALIGN_TO,
    CONF_X,
    CONF_Y,
    DIRECTIONS,
    LV_EVENT_MAP,
    LV_EVENT_TRIGGERS,
    SWIPE_TRIGGERS,
    literal,
)
from .lvcode import (
    API_EVENT,
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvConditional,
    lv,
    lv_add,
    lv_event_t_ptr,
    lvgl_static,
)
from .types import LV_EVENT
from .widgets import LvScrActType, get_scr_act, widget_map


async def generate_triggers():
    """
    Generate LVGL triggers for all defined widgets
    Must be done after all widgets completed
    """

    for w in widget_map.values():
        if isinstance(w.type, LvScrActType):
            w = get_scr_act(w.var)

        if w.config:
            for event, conf in {
                event: conf
                for event, conf in w.config.items()
                if event in LV_EVENT_TRIGGERS
            }.items():
                conf = conf[0]
                w.add_flag("LV_OBJ_FLAG_CLICKABLE")
                event = literal("LV_EVENT_" + LV_EVENT_MAP[event[3:].upper()])
                await add_trigger(conf, w, event)

            for event, conf in {
                event: conf
                for event, conf in w.config.items()
                if event in SWIPE_TRIGGERS
            }.items():
                conf = conf[0]
                dir = event[9:].upper()
                dir = {"UP": "TOP", "DOWN": "BOTTOM"}.get(dir, dir)
                dir = DIRECTIONS.mapper(dir)
                w.clear_flag("LV_OBJ_FLAG_SCROLLABLE")
                selected = literal(
                    f"lv_indev_get_gesture_dir(lv_indev_get_act()) == {dir}"
                )
                await add_trigger(
                    conf, w, literal("LV_EVENT_GESTURE"), is_selected=selected
                )

            for conf in w.config.get(CONF_ON_VALUE, ()):
                await add_trigger(
                    conf,
                    w,
                    LV_EVENT.VALUE_CHANGED,
                    API_EVENT,
                    UPDATE_EVENT,
                )

            # Generate align to directives while we're here
            if align_to := w.config.get(CONF_ALIGN_TO):
                target = widget_map[align_to[CONF_ID]].obj
                align = literal(align_to[CONF_ALIGN])
                x = align_to[CONF_X]
                y = align_to[CONF_Y]
                lv.obj_align_to(w.obj, target, align, x, y)


async def add_trigger(conf, w, *events, is_selected=None):
    is_selected = is_selected or w.is_selected()
    tid = conf[CONF_TRIGGER_ID]
    trigger = cg.new_Pvariable(tid)
    args = w.get_args() + [(lv_event_t_ptr, "event")]
    value = w.get_values()
    await automation.build_automation(trigger, args, conf)
    async with LambdaContext(EVENT_ARG, where=tid) as context:
        with LvConditional(is_selected):
            lv_add(trigger.trigger(*value, literal("event")))
    lv_add(lvgl_static.add_event_cb(w.obj, await context.get_lambda(), *events))
