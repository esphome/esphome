import sys
from typing import Any

from esphome import codegen as cg, config_validation as cv
from esphome.config_validation import Invalid
from esphome.const import CONF_GROUP, CONF_ID, CONF_STATE
from esphome.core import ID, TimePeriod
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import MockObjClass

from .defines import (
    CONF_DEFAULT,
    CONF_MAIN,
    CONF_SCROLLBAR_MODE,
    CONF_WIDGETS,
    OBJ_FLAGS,
    PARTS,
    STATES,
    LValidator,
    join_enums,
)
from .helpers import add_lv_use
from .lvcode import ConstantLiteral, add_line_marks, lv, lv_add, lv_assign, lv_obj
from .schemas import ALL_STYLES, STYLE_REMAP
from .types import WIDGET_TYPES, WidgetType, lv_obj_t

EVENT_LAMB = "event_lamb__"


class LvScrActType(WidgetType):
    """
    A "widget" representing the active screen.
    """

    def __init__(self):
        super().__init__("lv_scr_act()", lv_obj_t, ())

    def obj_creator(self, parent: MockObjClass, config: dict):
        return []

    async def to_code(self, w, config: dict):
        return []


class Widget:
    """
    Represents a Widget.
    """

    widgets_completed = False

    @staticmethod
    def set_completed():
        Widget.widgets_completed = True

    def __init__(self, var, wtype: WidgetType, config: dict = None, parent=None):
        self.var = var
        self.type = wtype
        self.config = config
        self.scale = 1.0
        self.step = 1.0
        self.range_from = -sys.maxsize
        self.range_to = sys.maxsize
        self.parent = parent

    @staticmethod
    def create(name, var, wtype: WidgetType, config: dict = None, parent=None):
        w = Widget(var, wtype, config, parent)
        if name is not None:
            widget_map[name] = w
        return w

    @property
    def obj(self):
        if self.type.is_compound():
            return f"{self.var}->obj"
        return self.var

    def add_state(self, *args):
        return lv_obj.add_state(self.obj, *args)

    def clear_state(self, *args):
        return lv_obj.clear_state(self.obj, *args)

    def add_flag(self, *args):
        return lv_obj.add_flag(self.obj, *args)

    def clear_flag(self, *args):
        return lv_obj.clear_flag(self.obj, *args)

    def set_property(self, prop, value, animated: bool = None, ltype=None):
        if isinstance(value, dict):
            value = value.get(prop)
        if value is None:
            return
        if isinstance(value, TimePeriod):
            value = value.total_milliseconds
        ltype = ltype or self.__type_base()
        if animated is None or self.type.animated is not True:
            lv.call(f"{ltype}_set_{prop}", self.obj, value)
        else:
            lv.call(
                f"{ltype}_set_{prop}",
                self.obj,
                value,
                "LV_ANIM_ON" if animated else "LV_ANIM_OFF",
            )

    def get_property(self, prop, ltype=None):
        ltype = ltype or self.__type_base()
        return f"lv_{ltype}_get_{prop}({self.obj})"

    def set_style(self, prop, value, state):
        if value is None:
            return []
        return lv.call(f"obj_set_style_{prop}", self.obj, value, state)

    def __type_base(self):
        wtype = self.type.w_type
        base = str(wtype)
        if base.startswith("Lv"):
            return f"{wtype}".removeprefix("Lv").removesuffix("Type").lower()
        return f"{wtype}".removeprefix("lv_").removesuffix("_t")

    def __str__(self):
        return f"({self.var}, {self.type})"


# Map of widgets to their config, used for trigger generation
widget_map: dict[Any, Widget] = {}


def get_widget_generator(wid):
    """
    Used to wait for a widget during code generation.
    :param wid:
    :return:
    """
    while True:
        if obj := widget_map.get(wid):
            return obj
        if Widget.widgets_completed:
            raise Invalid(
                f"Widget {wid} not found, yet all widgets should be defined by now"
            )
        yield


async def get_widget(wid: ID) -> Widget:
    if obj := widget_map.get(wid):
        return obj
    return await FakeAwaitable(get_widget_generator(wid))


def collect_props(config):
    """
    Collect all properties from a configuration
    :param config:
    :return:
    """
    props = {}
    for prop in [*ALL_STYLES, *OBJ_FLAGS, CONF_GROUP]:
        if prop in config:
            props[prop] = config[prop]
    return props


def collect_states(config):
    """
    Collect prperties for each state of a widget
    :param config:
    :return:
    """
    states = {CONF_DEFAULT: collect_props(config)}
    for state in STATES:
        if state in config:
            states[state] = collect_props(config[state])
    return states


def collect_parts(config):
    """
    Collect properties and states for all widget parts
    :param config:
    :return:
    """
    parts = {CONF_MAIN: collect_states(config)}
    for part in PARTS:
        if part in config:
            parts[part] = collect_states(config[part])
    return parts


async def set_obj_properties(w: Widget, config):
    """Generate a list of C++ statements to apply properties to an lv_obj_t"""
    parts = collect_parts(config)
    for part, states in parts.items():
        for state, props in states.items():
            lv_state = ConstantLiteral(
                f"(int)LV_STATE_{state.upper()}|(int)LV_PART_{part.upper()}"
            )
            for prop, value in {
                k: v for k, v in props.items() if k in ALL_STYLES
            }.items():
                if isinstance(ALL_STYLES[prop], LValidator):
                    value = await ALL_STYLES[prop].process(value)
                    # Remapping for backwards compatibility of style names
                prop_r = STYLE_REMAP.get(prop, prop)
                w.set_style(prop_r, value, lv_state)
    flag_clr = set()
    flag_set = set()
    props = parts[CONF_MAIN][CONF_DEFAULT]
    for prop, value in {k: v for k, v in props.items() if k in OBJ_FLAGS}.items():
        if value:
            flag_set.add(prop)
        else:
            flag_clr.add(prop)
    if flag_set:
        adds = join_enums(flag_set, "LV_OBJ_FLAG_")
        w.add_flag(adds)
    if flag_clr:
        clrs = join_enums(flag_clr, "LV_OBJ_FLAG_")
        w.clear_flag(clrs)

    if states := config.get(CONF_STATE):
        adds = set()
        clears = set()
        lambs = {}
        for key, value in states.items():
            if isinstance(value, cv.Lambda):
                lambs[key] = value
            elif value == "true":
                adds.add(key)
            else:
                clears.add(key)
        if adds:
            adds = join_enums(adds, "LV_STATE_")
            w.add_state(adds)
        if clears:
            clears = join_enums(clears, "LV_STATE_")
            w.clear_state(clears)
        for key, value in lambs.items():
            lamb = await cg.process_lambda(value, [], return_type=cg.bool_)
            state = ConstantLiteral(f"LV_STATE_{key.upper}")
            lv.cond_if(lamb)
            w.add_state(state)
            lv.cond_else()
            w.clear_state(state)
            lv.cond_endif()
    if scrollbar_mode := config.get(CONF_SCROLLBAR_MODE):
        lv_obj.set_scrollbar_mode(w.obj, scrollbar_mode)


async def add_widgets(parent: Widget, config: dict):
    """
    Add all widgets to an object
    :param parent: The enclosing obj
    :param config: The configuration
    :return:
    """
    for w in config.get(CONF_WIDGETS) or ():
        w_type, w_cnfig = next(iter(w.items()))
        await widget_to_code(w_cnfig, w_type, parent.obj)


async def widget_to_code(w_cnfig, w_type, parent):
    """
    Converts a Widget definition to C code.
    :param w_cnfig: The widget configuration
    :param w_type:  The Widget type
    :param parent: The parent to which the widget should be added
    :return:
    """
    spec: WidgetType = WIDGET_TYPES[w_type]
    creator = spec.obj_creator(parent, w_cnfig)
    add_lv_use(spec.name)
    add_lv_use(*spec.get_uses())
    wid = w_cnfig[CONF_ID]
    add_line_marks(wid)
    if spec.is_compound():
        var = cg.new_Pvariable(wid)
        lv_add(var.set_obj(creator))
    else:
        var = cg.Pvariable(wid, cg.nullptr, type_=lv_obj_t)
        lv_assign(var, creator)

    widget = Widget.create(wid, var, spec, w_cnfig, parent)
    await set_obj_properties(widget, w_cnfig)
    await add_widgets(widget, w_cnfig)
    await spec.to_code(widget, w_cnfig)
