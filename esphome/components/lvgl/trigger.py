from esphome import automation
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_ON_BOOT,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_X,
    CONF_Y,
)
from esphome.cpp_generator import MockObj, new_Pvariable
from esphome.cpp_helpers import register_component
from esphome.cpp_types import nullptr

from .defines import (
    CONF_ALIGN,
    CONF_ALIGN_TO,
    CONF_ALIGN_TO_LAMBDA_ID,
    DIRECTIONS,
    LV_DISPLAY_EVENT_MAP,
    LV_DISPLAY_EVENT_TRIGGERS,
    LV_EVENT_MAP,
    LV_EVENT_TRIGGERS,
    LV_SCREEN_EVENT_MAP,
    LV_SCREEN_EVENT_TRIGGERS,
    SWIPE_TRIGGERS,
    get_widget_map,
    is_press_event,
    literal,
)
from .lvcode import (
    EVENT_ARG,
    UPDATE_EVENT,
    LambdaContext,
    LvConditional,
    lv,
    lv_add,
    lv_expr,
    lvgl_static,
)
from .types import LV_EVENT, lv_point_t
from .widgets import LvScrActType, get_screen_active


async def add_on_boot_triggers(triggers):
    for conf in triggers:
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], 390)
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)


async def generate_triggers():
    """
    Generate LVGL triggers for all defined widgets
    Must be done after all widgets completed
    """

    all_triggers = (
        LV_EVENT_TRIGGERS + LV_DISPLAY_EVENT_TRIGGERS + LV_SCREEN_EVENT_TRIGGERS
    )
    for w in get_widget_map().values():
        config = w.config
        if isinstance(w.type, LvScrActType):
            w = get_screen_active(w.var)

        if config:
            for event, conf in {
                event: conf for event, conf in config.items() if event in all_triggers
            }.items():
                conf = conf[0]
                w.add_flag("LV_OBJ_FLAG_CLICKABLE")
                await add_trigger(conf, w, event)

            for event, conf in {
                event: conf for event, conf in config.items() if event in SWIPE_TRIGGERS
            }.items():
                conf = conf[0]
                dir = event[9:].upper()
                dir = {"UP": "TOP", "DOWN": "BOTTOM"}.get(dir, dir)
                dir = DIRECTIONS.mapper(dir)
                w.clear_flag("LV_OBJ_FLAG_SCROLLABLE")
                selected = literal(
                    f"lv_indev_get_gesture_dir(lv_indev_active()) == {dir}"
                )
                await add_trigger(conf, w, "GESTURE", is_selected=selected)

            for conf in config.get(CONF_ON_VALUE, ()):
                await add_trigger(
                    conf,
                    w,
                    LV_EVENT.VALUE_CHANGED,
                    UPDATE_EVENT,
                )

            await add_on_boot_triggers(config.get(CONF_ON_BOOT, ()))


async def generate_align_tos(config: dict):
    """
    Called once, with a full lvgl configuration to emit deferred align_to actions as a component
    that executes after the LVGL setup. This is required since align_to actions are not recalculated on layout changes
    and so must be applied after the display is properly laid out.
    :param config:
    :return:
    """
    widget_map = get_widget_map()
    align_tos = tuple(
        w for w in widget_map.values() if w.config and CONF_ALIGN_TO in w.config
    )
    if align_tos:
        async with LambdaContext(where="align_to") as context:
            for w in align_tos:
                align_to = w.config[CONF_ALIGN_TO]
                target = widget_map[align_to[CONF_ID]].obj
                align = literal(align_to[CONF_ALIGN])
                x = align_to[CONF_X]
                y = align_to[CONF_Y]
                lv.obj_align_to(w.obj, target, align, x, y)

            action_id = config[CONF_ALIGN_TO_LAMBDA_ID]
            var = new_Pvariable(action_id, await context.get_lambda())
            await register_component(var, {})


TRIGGER_MAP = LV_EVENT_MAP | LV_DISPLAY_EVENT_MAP | LV_SCREEN_EVENT_MAP
DISPLAY_TRIGGERS = set(LV_DISPLAY_EVENT_TRIGGERS)


def _get_event_literal(trigger: str | MockObj) -> MockObj:
    if isinstance(trigger, MockObj):
        return trigger
    trigger = trigger.removeprefix("on_")
    return literal("LV_EVENT_" + TRIGGER_MAP[trigger.upper()])


async def add_trigger(conf, w, *events: str | MockObj, is_selected=None):
    is_selected = is_selected or w.is_selected()
    tid = conf[CONF_TRIGGER_ID]
    trigger = cg.new_Pvariable(tid)
    args = w.get_args()
    value: list = w.get_values()
    if len(events) == 1 and is_press_event(str(events[0])):
        # Make the touch point available for selected events
        args.append((lv_point_t, "point"))
        value.append(lvgl_static.get_touch_relative_to_obj(w.obj))
    args.extend(EVENT_ARG)
    await automation.build_automation(trigger, args, conf)
    async with LambdaContext(EVENT_ARG, where=tid) as context:
        with LvConditional(is_selected):
            lv_add(trigger.trigger(*value, literal("event")))
    callback = await context.get_lambda()
    event_literals = [_get_event_literal(event) for event in events]
    if str(events[0]) in DISPLAY_TRIGGERS:
        assert len(events) == 1
        lv.display_add_event_cb(
            lv_expr.obj_get_display(w.obj), callback, event_literals[0], nullptr
        )
    else:
        lv_add(
            lvgl_static.add_event_cb(w.obj, await context.get_lambda(), *event_literals)
        )
