from esphome import automation
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_ON_VALUE, CONF_TRIGGER_ID

from .defines import (
    CONF_ALIGN,
    CONF_ALIGN_TO,
    CONF_X,
    CONF_Y,
    LV_EVENT,
    LV_EVENT_TRIGGERS,
    literal,
)
from .lvcode import LambdaContext, add_line_marks, lv, lv_add
from .widget import widget_map

lv_event_t_ptr = cg.global_ns.namespace("lv_event_t").operator("ptr")


async def generate_triggers(lv_component):
    """
    Generate LVGL triggers for all defined widgets
    Must be done after all widgets completed
    :param lv_component:  The parent component
    :return:
    """

    for w in widget_map.values():
        if w.config:
            for event, conf in {
                event: conf
                for event, conf in w.config.items()
                if event in LV_EVENT_TRIGGERS
            }.items():
                conf = conf[0]
                w.add_flag("LV_OBJ_FLAG_CLICKABLE")
                event = "LV_EVENT_" + LV_EVENT[event[3:].upper()]
                await add_trigger(conf, event, lv_component, w)
            for conf in w.config.get(CONF_ON_VALUE, ()):
                await add_trigger(conf, "LV_EVENT_VALUE_CHANGED", lv_component, w)

            # Generate align to directives while we're here
            if align_to := w.config.get(CONF_ALIGN_TO):
                target = widget_map[align_to[CONF_ID]].obj
                align = align_to[CONF_ALIGN]
                x = align_to[CONF_X]
                y = align_to[CONF_Y]
                lv.obj_align_to(w.obj, target, align, x, y)


async def add_trigger(conf, event, lv_component, w):
    tid = conf[CONF_TRIGGER_ID]
    add_line_marks(tid)
    trigger = cg.new_Pvariable(tid)
    args = w.get_args()
    value = w.get_value()
    await automation.build_automation(trigger, args, conf)
    with LambdaContext([(lv_event_t_ptr, "event_data")]) as context:
        add_line_marks(tid)
        lv_add(trigger.trigger(value))
    lv_add(lv_component.add_event_cb(w.obj, await context.get_lambda(), literal(event)))
