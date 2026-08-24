from collections.abc import Generator
from dataclasses import dataclass, field
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUTTON,
    CONF_ID,
    CONF_INDEX,
    CONF_ON_BOOT,
    CONF_ON_UPDATE,
    CONF_ON_VALUE,
    CONF_TEXT,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import MockObj
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from ..automation import action_to_code
from ..defines import (
    CONF_ALIGN_TO,
    CONF_MAIN,
    CONF_PAD_ROW,
    CONF_SCROLLBAR,
    CONF_WIDGETS,
    LV_EVENT_TRIGGERS,
    SWIPE_TRIGGERS,
    TYPE_FLEX,
    add_lv_use,
    literal,
)
from ..lv_validation import lv_int, lv_text, padding
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
from ..schemas import (
    ALL_STYLES,
    WIDGET_TYPES,
    any_widget_schema,
    apply_style_driven_defines,
    container_schema_value,
    remap_property,
)
from ..trigger import add_trigger
from ..types import LV_EVENT, LvType, ObjUpdateAction, lv_obj_t
from . import (
    Widget,
    WidgetType,
    apply_theme_styles,
    collect_parts,
    get_widgets,
    set_obj_properties,
)
from .buttonmatrix import CONF_BUTTONMATRIX
from .canvas import CONF_CANVAS
from .label import CONF_LABEL
from .meter import CONF_METER
from .tabview import CONF_TABVIEW
from .tileview import CONF_TILEVIEW

CONF_LIST = "list"
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
    list's own ID.
    """
    triggers_by_list = CORE.data.setdefault(DOMAIN, {})
    return triggers_by_list.setdefault(list_id, ListTriggers())


def _get_pending_list_triggers(list_id) -> ListTriggers:
    """
    Same shape as _get_list_triggers(), but holding raw on_add/on_remove automation
    configs, not yet built.
    """
    pending_by_list = CORE.data.setdefault(DOMAIN + "_pending", {})
    return pending_by_list.setdefault(list_id, ListTriggers())


def _list_triggers_completed_flag() -> list[bool]:
    return CORE.data.setdefault(DOMAIN + "_completed", [False])


def _list_triggers_completed_generator() -> Generator[None, None, None]:
    while True:
        if _list_triggers_completed_flag()[0]:
            return
        yield


async def _wait_list_triggers_completed() -> None:
    """Waits until finish_list_triggers() has built every list's on_add/on_remove automations."""
    if _list_triggers_completed_flag()[0]:
        return
    await FakeAwaitable(_list_triggers_completed_generator())


async def finish_list_triggers() -> None:
    """
    Builds every list's on_add/on_remove automations, collected by ListType.to_code()
    instead of being built there directly. Must run after set_widgets_completed(True).
    """
    for list_id, pending in CORE.data.get(DOMAIN + "_pending", {}).items():
        triggers = _get_list_triggers(list_id)
        for conf in pending.on_add:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [(cg.int_, "list_index")], conf)
            triggers.on_add.append(trigger)
        for conf in pending.on_remove:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [(cg.int_, "list_index")], conf)
            triggers.on_remove.append(trigger)
    _list_triggers_completed_flag()[0] = True


def _fire_index_triggers(triggers: list, index) -> None:
    for trigger in triggers:
        lv_add(trigger.trigger(index))


async def _fire_on_add(list_id, list_obj, entry_obj) -> None:
    await _wait_list_triggers_completed()
    triggers = _get_list_triggers(list_id).on_add
    if not triggers:
        return
    index = cg.RawExpression(f"lvgl::lv_list_get_row_index({list_obj}, {entry_obj})")
    _fire_index_triggers(triggers, index)


async def _fire_on_remove(list_id, index) -> None:
    await _wait_list_triggers_completed()
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
    """A plain wrapper around LVGL's native `lv_list`"""

    def __init__(self):
        super().__init__(
            CONF_LIST,
            lv_list_t,
            (CONF_MAIN, CONF_SCROLLBAR),
            LIST_CREATE_SCHEMA,
            modify_schema=LIST_SCHEMA,
        )

    def get_uses(self):
        return TYPE_FLEX, CONF_LABEL, CONF_BUTTON

    async def to_code(self, w: Widget, config: dict):
        on_add = config.get(CONF_ON_ADD, ())
        on_remove = config.get(CONF_ON_REMOVE, ())
        if not on_add and not on_remove:
            return
        pending = _get_pending_list_triggers(w.config[CONF_ID])
        pending.on_add.extend(on_add)
        pending.on_remove.extend(on_remove)


list_spec = ListType()

LIST_ID_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(lv_list_t)})


@automation.register_action(
    "lvgl.list.add_text",
    ObjUpdateAction,
    LIST_ID_SCHEMA.extend(
        {
            cv.Required(CONF_TEXT): lv_text,
            cv.Optional(CONF_INDEX): cv.templatable(cv.int_),
        }
    ),
    synchronous=True,
)
async def list_add_text_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_add_text(w: Widget):
        text = await lv_text.process(config[CONF_TEXT])
        with LocalVariable(
            "list_entry", lv_obj_t, lv_expr.list_add_text(w.obj, text)
        ) as entry:
            if (idx := config.get(CONF_INDEX)) is not None:
                lv.obj_move_to_index(entry, await lv_int.process(idx))
            await _fire_on_add(config[CONF_ID], w.obj, entry)

    return await action_to_code(
        widgets, do_add_text, action_id, template_arg, args, config
    )


_DYNAMIC_WIDGET_UNSUPPORTED = (
    CONF_BUTTONMATRIX,
    CONF_TABVIEW,
    CONF_TILEVIEW,
    CONF_METER,
    CONF_CANVAS,
)


def _check_dynamic_widget_supported(w_type_name: str, w_conf: dict) -> None:
    # Each of these allocates a Pvariable, or registers children into the global widget
    # map, once at boot - rebuilding them on every lvgl.list.add call would break that.
    if w_type_name in _DYNAMIC_WIDGET_UNSUPPORTED:
        raise cv.Invalid(
            f"'{w_type_name}' cannot be used with lvgl.list.add - it manages its own "
            "child widgets in a way that isn't compatible with widgets created at runtime"
        )
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        _check_dynamic_widget_supported(child_type, child_conf)


_UNSUPPORTED_DYNAMIC_KEYS = SWIPE_TRIGGERS + (CONF_ON_BOOT, CONF_ALIGN_TO)


def _check_no_unsupported_triggers(w_type_name: str, w_conf: dict) -> None:
    # These triggers currently aren't supporte for dynamic widgets
    for key in _UNSUPPORTED_DYNAMIC_KEYS:
        if key in w_conf:
            raise cv.Invalid(
                f"'{key}' is not supported on a widget added via lvgl.list.add - it "
                "would validate but generate nothing, since it's only wired for "
                "widgets that exist at boot",
                path=[w_type_name, key],
            )
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        _check_no_unsupported_triggers(child_type, child_conf)


def _check_no_explicit_widget_id(raw_value: dict) -> None:
    for w_type_name, w_conf in raw_value.items():
        if not isinstance(w_conf, dict):
            continue
        if CONF_ID in w_conf:
            raise cv.Invalid(
                "'id' is not allowed on a widget added via lvgl.list.add - it is "
                "rebuilt fresh on every call and never registered anywhere it "
                "could be looked up by",
                path=[w_type_name, CONF_ID],
            )
        for child in w_conf.get(CONF_WIDGETS, ()):
            if isinstance(child, dict):
                _check_no_explicit_widget_id(child)


@schema_extractor("schema")
def list_add_schema(value: Any) -> Any:
    # A plain cv.Schema can't express "id, an optional index, plus exactly one arbitrary
    # widget-type key", since the set of widget types isn't fixed until validation time.
    if value is SCHEMA_EXTRACT:
        return LIST_ID_SCHEMA.extend(
            {
                cv.Optional(CONF_INDEX): cv.templatable(cv.int_),
                **{
                    cv.Optional(name): container_schema_value(widget_type)
                    for name, widget_type in WIDGET_TYPES.items()
                },
            }
        )
    if not isinstance(value, dict):
        raise cv.Invalid("Expected a mapping")
    value = value.copy()
    if CONF_ID not in value:
        raise cv.Invalid(f"required key '{CONF_ID}' not provided")
    with cv.prepend_path([CONF_ID]):
        list_id = cv.use_id(lv_list_t)(value.pop(CONF_ID))
    result = {CONF_ID: list_id}
    if CONF_INDEX in value:
        with cv.prepend_path([CONF_INDEX]):
            result[CONF_INDEX] = cv.templatable(cv.int_)(value.pop(CONF_INDEX))
    if len(value) != 1:
        raise cv.Invalid(
            "lvgl.list.add takes exactly one widget definition, e.g. 'label:' or 'button:', alongside 'id' and optional 'index'"
        )
    _check_no_explicit_widget_id(value)
    result[CONF_WIDGET] = any_widget_schema()(value)
    [(w_type_name, w_conf)] = result[CONF_WIDGET][0].items()
    _check_dynamic_widget_supported(w_type_name, w_conf)
    _check_no_unsupported_triggers(w_type_name, w_conf)
    return result


def _register_lv_uses(w_type_name: str, w_conf: dict) -> None:
    # Must run before this coroutine's first await.
    widget_type = WIDGET_TYPES[w_type_name]
    add_lv_use(w_type_name)
    add_lv_use(*widget_type.get_uses())
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        _register_lv_uses(child_type, child_conf)


def _register_dynamic_widget_style_uses(w_conf: dict) -> None:
    props = {
        remap_property(prop)
        for part_states in collect_parts(w_conf).values()
        for state_props in part_states.values()
        for prop in state_props
        if prop in ALL_STYLES
    }
    apply_style_driven_defines(props)
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(_, child_conf)] = child.items()
        _register_dynamic_widget_style_uses(child_conf)


@automation.register_action(
    "lvgl.list.add",
    ObjUpdateAction,
    list_add_schema,
    synchronous=True,
)
async def list_add_to_code(config, action_id, template_arg, args):
    [(w_type_name, w_conf)] = config[CONF_WIDGET][0].items()
    _register_lv_uses(w_type_name, w_conf)
    _register_dynamic_widget_style_uses(w_conf)
    widgets = await get_widgets(config)

    async def do_add(w: Widget):
        index = None
        if (idx := config.get(CONF_INDEX)) is not None:
            index = await lv_int.process(idx)
        await _build_dynamic_widget(
            w_type_name,
            w_conf,
            w.obj,
            config[CONF_ID],
            w.obj,
            top_level=True,
            index=index,
        )

    return await action_to_code(widgets, do_add, action_id, template_arg, args, config)


async def _build_dynamic_widget(
    w_type_name: str,
    w_conf: dict,
    parent,
    list_id,
    list_obj,
    top_level: bool = False,
    index=None,
    depth: int = 0,
) -> None:
    # Builds one widget (recursively, with children and triggers) as a LocalVariable
    # instead of a global Pvariable. Compound
    # widgets are heap-allocated and freed via LV_EVENT_DELETE.
    # `depth` suffixes the local variable's name below the row's top level.
    widget_type = WIDGET_TYPES[w_type_name]
    var_name = f"dyn_{w_type_name}" if depth == 0 else f"dyn_{w_type_name}_{depth}"
    add_lv_use(w_type_name)
    add_lv_use(*widget_type.get_uses())

    async def finish_and_fire(w: Widget) -> None:
        # Shared tail for both branches below - must run while var's LocalVariable
        # block (opened by whichever branch calls this) is still open
        await _finish_dynamic_widget(w, w_conf, list_id, list_obj, depth)
        if top_level:
            if index is not None:
                lv.obj_move_to_index(w.obj, index)
            await _fire_on_add(list_id, list_obj, w.obj)

    if widget_type.is_compound():
        with LocalVariable(
            var_name, widget_type.w_type, widget_type.w_type.new()
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
            await finish_and_fire(w)
    else:
        creator = await widget_type.obj_creator(parent, w_conf)
        with LocalVariable(var_name, lv_obj_t, creator) as var:
            w = Widget(var, widget_type, w_conf)
            await finish_and_fire(w)


async def _finish_dynamic_widget(
    w: Widget, w_conf: dict, list_id, list_obj, depth: int = 0
) -> None:
    await w.type.on_create(w.obj, w_conf)
    apply_theme_styles(w)
    await set_obj_properties(w, w_conf)
    await w.type.to_code(w, w_conf)
    await _wire_dynamic_triggers(w, w_conf)
    for child in w_conf.get(CONF_WIDGETS, ()):
        [(child_type, child_conf)] = child.items()
        await _build_dynamic_widget(
            child_type, child_conf, w.obj, list_id, list_obj, depth=depth + 1
        )


async def _wire_dynamic_triggers(w: Widget, config: dict) -> None:
    # Mirrors generate_triggers(), but runs immediately
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


LIST_REMOVE_SCHEMA = LIST_ID_SCHEMA.extend(
    {
        # positive_int, not int_: a negative index would silently delete the *last*
        # row (lv_obj_get_child() counts back from the end) while reporting that
        # same bogus value to on_remove's list_index.
        cv.Required(CONF_INDEX): cv.templatable(cv.positive_int),
    }
)


@automation.register_action(
    "lvgl.list.remove",
    ObjUpdateAction,
    LIST_REMOVE_SCHEMA,
    synchronous=True,
)
async def list_remove_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_remove(w: Widget):
        index = await lv_int.process(config[CONF_INDEX])
        # Materialised into a local since index is needed at two call sites below, and
        # a lambda's body gets re-emitted (and re-run) at every point it's used.
        with (
            LocalVariable("list_index", cg.int_, index, modifier="") as idx,
            # Out-of-range lookup/log lives in a shared C++ helper, not inline here:
            # a config can have many lvgl.list.remove call sites.
            LocalVariable(
                "list_child",
                lv_obj_t,
                cg.RawExpression(f"lvgl::lv_list_get_row_for_remove({w.obj}, {idx})"),
            ) as child,
            LvConditional(child),
        ):
            await _fire_on_remove(config[CONF_ID], idx)
            # Recursively destroys the whole subtree
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
        await _wait_list_triggers_completed()
        triggers = _get_list_triggers(config[CONF_ID]).on_remove
        if triggers:
            # Fire on_remove for every entry, newest to oldest, before wiping them all out,
            # so on_remove's semantics ("an entry left the list") hold
            with LvCountdown("list_index", lv_expr.obj_get_child_count(w.obj)) as index:
                _fire_index_triggers(triggers, index)
        # lv_obj_clean recursively destroys every child's whole subtree
        lv.obj_clean(w.obj)

    return await action_to_code(
        widgets, do_clear, action_id, template_arg, args, config
    )
