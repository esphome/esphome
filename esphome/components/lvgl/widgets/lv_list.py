from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INDEX,
    CONF_ON_UPDATE,
    CONF_ON_VALUE,
    CONF_TEXT,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.cpp_types import nullptr
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from ..automation import action_to_code
from ..defines import (
    CONF_MAIN,
    CONF_PAD_ROW,
    CONF_SCROLLBAR,
    CONF_WIDGETS,
    LV_EVENT_TRIGGERS,
    TYPE_FLEX,
    add_lv_use,
    literal,
)
from ..lv_validation import lv_bool, lv_int, lv_text, padding
from ..lvcode import (
    UPDATE_EVENT,
    LocalVariable,
    LvConditional,
    LvCountdown,
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
CONF_ON_ADD = "on_add"
CONF_ON_REMOVE = "on_remove"

DOMAIN = "lvgl_list"

lv_list_t = LvType("lv_list_t")


@dataclass
class ListTriggers:
    on_add: list = field(default_factory=list)
    on_remove: list = field(default_factory=list)


def _get_list_triggers(list_id) -> ListTriggers:
    """
    Trigger Pvariables built for a given list's `on_add`/`on_remove` config, indexed by the
    list's own ID so that the `lvgl.list.*` actions (generated independently, wherever they're
    referenced in the config) can find and fire them.
    """
    triggers_by_list = CORE.data.setdefault(DOMAIN, {})
    return triggers_by_list.setdefault(list_id, ListTriggers())


def _fire_index_triggers(triggers: list, index) -> None:
    for trigger in triggers:
        lv_add(trigger.trigger(index))


def _fire_on_add(list_id, list_obj, entry_obj) -> None:
    triggers = _get_list_triggers(list_id).on_add
    if not triggers:
        return
    index = cg.RawExpression(f"lvgl::lv_list_get_row_index({list_obj}, {entry_obj})")
    _fire_index_triggers(triggers, index)


def _fire_on_remove(list_id, index) -> None:
    _fire_index_triggers(_get_list_triggers(list_id).on_remove, index)


LIST_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PAD_ROW): padding,
    }
)

LIST_CREATE_SCHEMA = LIST_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_ADD): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template(cg.int_)
                ),
            }
        ),
        cv.Optional(CONF_ON_REMOVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template(cg.int_)
                ),
            }
        ),
    }
)


class ListType(WidgetType):
    """
    A plain wrapper around LVGL's native `lv_list`: a scrollable container meant to be
    populated at runtime, via the `lvgl.list.add_text`/`add_button`/`add`/`remove`/`clear`
    actions, rather than a static `options:`-style list -- for content that isn't known
    until the device is running (sensor readings, WiFi scan results, and so on).
    """

    def __init__(self):
        super().__init__(
            CONF_LIST,
            lv_list_t,
            (CONF_MAIN, CONF_SCROLLBAR),
            LIST_CREATE_SCHEMA,
            modify_schema=LIST_SCHEMA,
        )

    def get_uses(self):
        return (TYPE_FLEX,)

    async def to_code(self, w: Widget, config: dict):
        # `to_code` is also invoked (with the update schema's config, which has no
        # on_add/on_remove) whenever `lvgl.list.update` runs against this widget - so these
        # `.get`s are always empty then, and `w.config` (the widget's own, stable creation
        # config) rather than `config` must be used for the trigger-store key below, since
        # the update schema's `id` is a list of ids, not a single one.
        on_add = config.get(CONF_ON_ADD, ())
        on_remove = config.get(CONF_ON_REMOVE, ())
        if not on_add and not on_remove:
            return
        triggers = _get_list_triggers(w.config[CONF_ID])
        for conf in on_add:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [(cg.int_, "list_index")], conf)
            triggers.on_add.append(trigger)
        for conf in on_remove:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [(cg.int_, "list_index")], conf)
            triggers.on_remove.append(trigger)


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
        with LocalVariable(
            "list_entry", lv_obj_t, lv_expr.list_add_text(w.obj, text)
        ) as entry:
            _fire_on_add(config[CONF_ID], w.obj, entry)

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
        with LocalVariable(
            "list_btn", lv_obj_t, lv_expr.list_add_button(w.obj, nullptr, text)
        ) as btn:
            with LvConditional(checkable):
                lv_obj.add_flag(btn, literal("LV_OBJ_FLAG_CHECKABLE"))
            _fire_on_add(config[CONF_ID], w.obj, btn)

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
        await _build_dynamic_widget(
            w_type_name, w_conf, w.obj, config[CONF_ID], w.obj, top_level=True
        )

    return await action_to_code(widgets, do_add, action_id, template_arg, args, config)


async def _build_dynamic_widget(
    w_type_name: str,
    w_conf: dict,
    parent,
    list_id,
    list_obj,
    top_level: bool = False,
) -> None:
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
            await _finish_dynamic_widget(w, w_conf, list_id, list_obj)
            if top_level:
                _fire_on_add(list_id, list_obj, w.obj)
    else:
        creator = await widget_type.obj_creator(parent, w_conf)
        with LocalVariable(f"dyn_{w_type_name}", lv_obj_t, creator) as var:
            w = Widget(var, widget_type, w_conf)
            await _finish_dynamic_widget(w, w_conf, list_id, list_obj)
            if top_level:
                _fire_on_add(list_id, list_obj, w.obj)


async def _finish_dynamic_widget(w: Widget, w_conf: dict, list_id, list_obj) -> None:
    await w.type.on_create(w.obj, w_conf)
    await set_obj_properties(w, w_conf)
    await w.type.to_code(w, w_conf)
    await _wire_dynamic_triggers(w, w_conf)
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        await _build_dynamic_widget(child_type, child_conf, w.obj, list_id, list_obj)


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
    #
    # Every trigger wired here already receives the standard `event` lambda parameter
    # (an `lv_event_t *`, same as any other widget's triggers); combined with
    # `lvgl::lv_list_get_row_index(id(the_list), lv_event_get_target(event))`, a lambda
    # in the trigger can recover which list row this widget belongs to, however deeply
    # it's nested inside it - the Trigger<Ts...> types built generically for every
    # widget type by schemas.py's automation_schema() have no room for an extra
    # "list_index" argument, since the exact same schema/type is shared by that widget
    # type everywhere else in the config, not just inside list rows.
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
            _fire_on_remove(config[CONF_ID], index)
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
        triggers = _get_list_triggers(config[CONF_ID]).on_remove
        if triggers:
            # Fire on_remove for every entry, newest to oldest, before wiping them all out,
            # so on_remove's semantics ("an entry left the list") hold regardless of whether
            # it left via lvgl.list.remove or lvgl.list.clear.
            with LvCountdown("list_index", lv_expr.obj_get_child_count(w.obj)) as index:
                _fire_index_triggers(triggers, index)
        # lv_obj_clean recursively destroys every child's whole subtree, same as
        # lv_obj_del does for a single entry in lvgl.list.remove.
        lv.obj_clean(w.obj)

    return await action_to_code(
        widgets, do_clear, action_id, template_arg, args, config
    )
