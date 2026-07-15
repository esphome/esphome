from esphome import automation
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INDEX, CONF_ON_UPDATE, CONF_ON_VALUE, CONF_TEXT
from esphome.cpp_generator import MockObj
from esphome.cpp_types import nullptr
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from ..automation import action_to_code
from ..defines import (
    CONF_MAIN,
    CONF_SCROLLBAR,
    CONF_WIDGETS,
    LV_EVENT_TRIGGERS,
    TYPE_FLEX,
    add_lv_use,
    literal,
)
from ..lv_validation import lv_bool, lv_int, lv_text
from ..lvcode import (
    UPDATE_EVENT,
    LocalVariable,
    LvConditional,
    lv,
    lv_add,
    lv_expr,
    lv_obj,
)
from ..schemas import WIDGET_TYPES, any_widget_schema, container_schema_value
from ..trigger import add_trigger
from ..types import LV_EVENT, LvType, ObjUpdateAction, lv_obj_t
from . import Widget, WidgetType, get_widgets, set_obj_properties

CONF_LIST = "list"
CONF_CHECKABLE = "checkable"
CONF_WIDGET = "widget"

lv_list_t = LvType("lv_list_t")


class ListType(WidgetType):
    """
    A plain wrapper around LVGL's native `lv_list`: a scrollable container meant to be
    populated at runtime, via the `lvgl.list.add_text`/`add_button`/`add`/`remove`/`clear`
    actions, rather than a static `options:`-style list -- for content that isn't known
    until the device is running (sensor readings, WiFi scan results, and so on).
    """

    def __init__(self):
        super().__init__(CONF_LIST, lv_list_t, (CONF_MAIN, CONF_SCROLLBAR), {})

    def get_uses(self):
        return (TYPE_FLEX,)


list_spec = ListType()

LIST_ID_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(lv_list_t)})


@automation.register_action(
    "lvgl.list.add_text",
    ObjUpdateAction,
    LIST_ID_SCHEMA.extend({cv.Required(CONF_TEXT): lv_text}),
    synchronous=True,
)
async def list_add_text_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_add_text(w: Widget):
        text = await lv_text.process(config[CONF_TEXT])
        lv.list_add_text(w.obj, text)

    return await action_to_code(
        widgets, do_add_text, action_id, template_arg, args, config
    )


@automation.register_action(
    "lvgl.list.add_button",
    ObjUpdateAction,
    LIST_ID_SCHEMA.extend(
        {
            cv.Required(CONF_TEXT): lv_text,
            cv.Optional(CONF_CHECKABLE, default=False): lv_bool,
        }
    ),
    synchronous=True,
)
async def list_add_button_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_add_button(w: Widget):
        text = await lv_text.process(config[CONF_TEXT])
        checkable = await lv_bool.process(config[CONF_CHECKABLE])
        with (
            LocalVariable(
                "list_btn", lv_obj_t, lv_expr.list_add_button(w.obj, nullptr, text)
            ) as btn,
            LvConditional(checkable),
        ):
            lv_obj.add_flag(btn, literal("LV_OBJ_FLAG_CHECKABLE"))

    return await action_to_code(
        widgets, do_add_button, action_id, template_arg, args, config
    )


@schema_extractor("schema")
def list_add_schema(value):
    # A plain cv.Schema can't express "id, plus exactly one arbitrary widget-type
    # key" - any_widget_schema() only knows how to validate the widget part, and
    # doesn't exist as a fixed set of keys until actual validation time (widget
    # types register themselves as their modules get imported). So id is stripped
    # out by hand here, and everything else - whatever single widget-type key
    # remains - is handed to any_widget_schema() dynamically.
    if value is SCHEMA_EXTRACT:
        return LIST_ID_SCHEMA.extend(
            {
                cv.Optional(name): container_schema_value(widget_type)
                for name, widget_type in WIDGET_TYPES.items()
            }
        )
    if not isinstance(value, dict):
        raise cv.Invalid("Expected a mapping")
    value = value.copy()
    if CONF_ID not in value:
        raise cv.Invalid(f"required key '{CONF_ID}' not provided")
    with cv.prepend_path([CONF_ID]):
        list_id = cv.use_id(lv_list_t)(value.pop(CONF_ID))
    if len(value) != 1:
        raise cv.Invalid(
            "lvgl.list.add takes exactly one widget definition, e.g. 'label:' or 'button:', alongside 'id'"
        )
    return {CONF_ID: list_id, CONF_WIDGET: any_widget_schema()(value)}


@automation.register_action(
    "lvgl.list.add",
    ObjUpdateAction,
    list_add_schema,
    synchronous=True,
)
async def list_add_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_add(w: Widget):
        [(w_type_name, w_conf)] = config[CONF_WIDGET][0].items()
        await _build_dynamic_widget(w_type_name, w_conf, w.obj)

    return await action_to_code(widgets, do_add, action_id, template_arg, args, config)


async def _build_dynamic_widget(w_type_name: str, w_conf: dict, parent) -> None:
    # Builds one widget (recursively, with children and triggers) using a LocalVariable
    # instead of the usual global Pvariable, so this can run fresh on every call instead
    # of once at boot. Compound widgets are heap-allocated (rather than placement-new'd
    # into static storage, which only tolerates a single boot-time construction) and
    # freed via an LV_EVENT_DELETE callback, since lv_obj_del()/lv_obj_clean() only know
    # how to destroy LVGL's own object tree, not a separate C++ wrapper paired with it.
    widget_type = WIDGET_TYPES[w_type_name]
    add_lv_use(w_type_name)
    add_lv_use(*widget_type.get_uses())
    if widget_type.is_compound():
        with LocalVariable(
            f"dyn_{w_type_name}", widget_type.w_type, widget_type.w_type.new()
        ) as var:
            creator = await widget_type.obj_creator(parent, w_conf)
            lv_add(var.set_obj(creator))
            w = Widget(var, widget_type, w_conf)
            lv_obj.add_event_cb(
                w.obj,
                literal(f"lvgl::delete_lv_compound_on_delete<{widget_type.w_type}>"),
                literal("LV_EVENT_DELETE"),
                var,
            )
            await _finish_dynamic_widget(w, w_conf)
    else:
        creator = await widget_type.obj_creator(parent, w_conf)
        with LocalVariable(f"dyn_{w_type_name}", lv_obj_t, creator) as var:
            w = Widget(var, widget_type, w_conf)
            await _finish_dynamic_widget(w, w_conf)


async def _finish_dynamic_widget(w: Widget, w_conf: dict) -> None:
    await w.type.on_create(w.obj, w_conf)
    await set_obj_properties(w, w_conf)
    await w.type.to_code(w, w_conf)
    await _wire_dynamic_triggers(w, w_conf)
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        await _build_dynamic_widget(child_type, child_conf, w.obj)


async def _wire_dynamic_triggers(w: Widget, config: dict) -> None:
    # Mirrors trigger.py's generate_triggers(), but wires each trigger immediately
    # instead of deferring to that boot-time-only pass, which never sees widgets
    # created here. add_trigger's callback is a captureless C function pointer, so it
    # can't reference our LocalVariable-based w.var/w.obj from inside the callback
    # body (still valid, and still used, for registering the callback outside of it,
    # via attach_obj) - give it a proxy widget that recovers the object from the event
    # instead: for a compound widget, the wrapper (passed through as user_data, since
    # lv_event_get_target only ever hands back the plain lv_obj_t*); otherwise the
    # lv_obj_t* itself, via lv_event_get_target.
    if w.type.is_compound():
        event_var = MockObj(
            f"static_cast<{w.type.w_type} *>(lv_event_get_user_data(event))", "->"
        )
        user_data = w.var
    else:
        event_var = literal("static_cast<lv_obj_t *>(lv_event_get_target(event))")
        user_data = None
    event_target = Widget(event_var, w.type, config)
    for event, conf in {
        event: conf for event, conf in config.items() if event in LV_EVENT_TRIGGERS
    }.items():
        w.add_flag("LV_OBJ_FLAG_CLICKABLE")
        await add_trigger(
            conf[0], event_target, event, attach_obj=w.obj, user_data=user_data
        )
    for conf in config.get(CONF_ON_VALUE, ()):
        await add_trigger(
            conf,
            event_target,
            LV_EVENT.VALUE_CHANGED,
            UPDATE_EVENT,
            attach_obj=w.obj,
            user_data=user_data,
        )
    for conf in config.get(CONF_ON_UPDATE, ()):
        await add_trigger(
            conf, event_target, UPDATE_EVENT, attach_obj=w.obj, user_data=user_data
        )


@automation.register_action(
    "lvgl.list.remove",
    ObjUpdateAction,
    LIST_ID_SCHEMA.extend({cv.Required(CONF_INDEX): cv.templatable(cv.int_)}),
    synchronous=True,
)
async def list_remove_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_remove(w: Widget):
        index = await lv_int.process(config[CONF_INDEX])
        # lv_obj_del recursively destroys the whole subtree under the removed entry,
        # so a hierarchy added via lvgl.list.add is always cleaned up in full.
        with (
            LocalVariable(
                "list_child", lv_obj_t, lv_expr.obj_get_child(w.obj, index)
            ) as child,
            LvConditional(child),
        ):
            lv.obj_del(child)

    return await action_to_code(
        widgets, do_remove, action_id, template_arg, args, config
    )


@automation.register_action(
    "lvgl.list.clear",
    ObjUpdateAction,
    LIST_ID_SCHEMA,
    synchronous=True,
)
async def list_clear_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_clear(w: Widget):
        # lv_obj_clean recursively destroys every child's whole subtree, same as
        # lv_obj_del does for a single entry in lvgl.list.remove.
        lv.obj_clean(w.obj)

    return await action_to_code(
        widgets, do_clear, action_id, template_arg, args, config
    )
