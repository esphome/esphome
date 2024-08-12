import sys
from typing import Any, Union

from esphome import codegen as cg, config_validation as cv
from esphome.config_validation import Invalid
from esphome.const import CONF_GROUP, CONF_ID, CONF_STATE, CONF_TYPE
from esphome.core import ID, TimePeriod
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import CallExpression, MockObj

from ..defines import (
    CONF_DEFAULT,
    CONF_FLEX_ALIGN_CROSS,
    CONF_FLEX_ALIGN_MAIN,
    CONF_FLEX_ALIGN_TRACK,
    CONF_FLEX_FLOW,
    CONF_GRID_COLUMN_ALIGN,
    CONF_GRID_COLUMNS,
    CONF_GRID_ROW_ALIGN,
    CONF_GRID_ROWS,
    CONF_LAYOUT,
    CONF_MAIN,
    CONF_SCROLLBAR_MODE,
    CONF_STYLES,
    CONF_WIDGETS,
    OBJ_FLAGS,
    PARTS,
    STATES,
    TYPE_FLEX,
    TYPE_GRID,
    LValidator,
    join_enums,
    literal,
)
from ..helpers import add_lv_use
from ..lvcode import (
    LvConditional,
    add_line_marks,
    lv,
    lv_add,
    lv_assign,
    lv_expr,
    lv_obj,
    lv_Pvariable,
)
from ..schemas import ALL_STYLES, STYLE_REMAP, WIDGET_TYPES
from ..types import LV_STATE, LvType, WidgetType, lv_coord_t, lv_obj_t, lv_obj_t_ptr

EVENT_LAMB = "event_lamb__"

theme_widget_map = {}


class LvScrActType(WidgetType):
    """
    A "widget" representing the active screen.
    """

    def __init__(self):
        super().__init__("lv_scr_act()", lv_obj_t, ())

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

    def __init__(self, var, wtype: WidgetType, config: dict = None):
        self.var = var
        self.type = wtype
        self.config = config
        self.scale = 1.0
        self.step = 1.0
        self.range_from = -sys.maxsize
        self.range_to = sys.maxsize
        if wtype.is_compound():
            self.obj = MockObj(f"{self.var}->obj")
        else:
            self.obj = var

    @staticmethod
    def create(name, var, wtype: WidgetType, config: dict = None):
        w = Widget(var, wtype, config)
        if name is not None:
            widget_map[name] = w
        return w

    def add_state(self, state):
        return lv_obj.add_state(self.obj, literal(state))

    def clear_state(self, state):
        return lv_obj.clear_state(self.obj, literal(state))

    def has_state(self, state):
        return (lv_expr.obj_get_state(self.obj) & literal(state)) != 0

    def is_pressed(self):
        return self.has_state(LV_STATE.PRESSED)

    def is_checked(self):
        return self.has_state(LV_STATE.CHECKED)

    def add_flag(self, flag):
        return lv_obj.add_flag(self.obj, literal(flag))

    def clear_flag(self, flag):
        return lv_obj.clear_flag(self.obj, literal(flag))

    async def set_property(self, prop, value, animated: bool = None):
        if isinstance(value, dict):
            value = value.get(prop)
            if isinstance(ALL_STYLES.get(prop), LValidator):
                value = await ALL_STYLES[prop].process(value)
            else:
                value = literal(value)
        if value is None:
            return
        if isinstance(value, TimePeriod):
            value = value.total_milliseconds
        if isinstance(value, str):
            value = literal(value)
        if animated is None or self.type.animated is not True:
            lv.call(f"{self.type.lv_name}_set_{prop}", self.obj, value)
        else:
            lv.call(
                f"{self.type.lv_name}_set_{prop}",
                self.obj,
                value,
                literal("LV_ANIM_ON" if animated else "LV_ANIM_OFF"),
            )

    def get_property(self, prop, ltype=None):
        ltype = ltype or self.__type_base()
        return cg.RawExpression(f"lv_{ltype}_get_{prop}({self.obj})")

    def set_style(self, prop, value, state):
        if value is None:
            return
        lv.call(f"obj_set_style_{prop}", self.obj, value, state)

    def __type_base(self):
        wtype = self.type.w_type
        base = str(wtype)
        if base.startswith("Lv"):
            return f"{wtype}".removeprefix("Lv").removesuffix("Type").lower()
        return f"{wtype}".removeprefix("lv_").removesuffix("_t")

    def __str__(self):
        return f"({self.var}, {self.type})"

    def get_args(self):
        if isinstance(self.type.w_type, LvType):
            return self.type.w_type.args
        return [(lv_obj_t_ptr, "obj")]

    def get_value(self):
        if isinstance(self.type.w_type, LvType):
            return self.type.w_type.value(self)
        return self.obj

    def get_number_value(self):
        value = self.type.mock_obj.get_value(self.obj)
        if self.scale == 1.0:
            return value
        return value / float(self.scale)

    def is_selected(self):
        """
        Overridable property to determine if the widget is selected. Will be None except
        for matrix buttons
        :return:
        """
        return None

    def get_max(self):
        return self.type.get_max(self.config)

    def get_min(self):
        return self.type.get_min(self.config)

    def get_step(self):
        return self.type.get_step(self.config)

    def get_scale(self):
        return self.type.get_scale(self.config)


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


async def get_widget_(wid: Widget):
    if obj := widget_map.get(wid):
        return obj
    return await FakeAwaitable(get_widget_generator(wid))


async def get_widgets(config: Union[dict, list], id: str = CONF_ID) -> list[Widget]:
    if not config:
        return []
    if not isinstance(config, list):
        config = [config]
    return [await get_widget_(c[id]) for c in config if id in c]


def collect_props(config):
    """
    Collect all properties from a configuration
    :param config:
    :return:
    """
    props = {}
    for prop in [*ALL_STYLES, *OBJ_FLAGS, CONF_STYLES, CONF_GROUP]:
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
    if layout := config.get(CONF_LAYOUT):
        layout_type: str = layout[CONF_TYPE]
        add_lv_use(layout_type)
        lv_obj.set_layout(w.obj, literal(f"LV_LAYOUT_{layout_type.upper()}"))
        if layout_type == TYPE_GRID:
            wid = config[CONF_ID]
            rows = [str(x) for x in layout[CONF_GRID_ROWS]]
            rows = "{" + ",".join(rows) + ", LV_GRID_TEMPLATE_LAST}"
            row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=lv_coord_t)
            row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
            w.set_style("grid_row_dsc_array", row_array, 0)
            columns = [str(x) for x in layout[CONF_GRID_COLUMNS]]
            columns = "{" + ",".join(columns) + ", LV_GRID_TEMPLATE_LAST}"
            column_id = ID(f"{wid}_column_dsc", is_declaration=True, type=lv_coord_t)
            column_array = cg.static_const_array(column_id, cg.RawExpression(columns))
            w.set_style("grid_column_dsc_array", column_array, 0)
            w.set_style(
                CONF_GRID_COLUMN_ALIGN, literal(layout.get(CONF_GRID_COLUMN_ALIGN)), 0
            )
            w.set_style(
                CONF_GRID_ROW_ALIGN, literal(layout.get(CONF_GRID_ROW_ALIGN)), 0
            )
        if layout_type == TYPE_FLEX:
            lv_obj.set_flex_flow(w.obj, literal(layout[CONF_FLEX_FLOW]))
            main = literal(layout[CONF_FLEX_ALIGN_MAIN])
            cross = literal(layout[CONF_FLEX_ALIGN_CROSS])
            track = literal(layout[CONF_FLEX_ALIGN_TRACK])
            lv_obj.set_flex_align(w.obj, main, cross, track)
    parts = collect_parts(config)
    for part, states in parts.items():
        for state, props in states.items():
            lv_state = join_enums((f"LV_STATE_{state}", f"LV_PART_{part}"))
            for style_id in props.get(CONF_STYLES, ()):
                lv_obj.add_style(w.obj, MockObj(style_id), lv_state)
            for prop, value in {
                k: v for k, v in props.items() if k in ALL_STYLES
            }.items():
                if isinstance(ALL_STYLES[prop], LValidator):
                    value = await ALL_STYLES[prop].process(value)
                prop_r = STYLE_REMAP.get(prop, prop)
                w.set_style(prop_r, value, lv_state)
    if group := config.get(CONF_GROUP):
        group = await cg.get_variable(group)
        lv.group_add_obj(group, w.obj)
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
            elif value:
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
            state = f"LV_STATE_{key.upper()}"
            with LvConditional(f"{lamb}()") as cond:
                w.add_state(state)
                cond.else_()
                w.clear_state(state)
    await w.set_property(CONF_SCROLLBAR_MODE, config)


async def add_widgets(parent: Widget, config: dict):
    """
    Add all widgets to an object
    :param parent: The enclosing obj
    :param config: The configuration
    :return:
    """
    for w in config.get(CONF_WIDGETS, ()):
        w_type, w_cnfig = next(iter(w.items()))
        await widget_to_code(w_cnfig, w_type, parent.obj)


async def widget_to_code(w_cnfig, w_type: WidgetType, parent):
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
        var = lv_Pvariable(lv_obj_t, wid)
        lv_assign(var, creator)

    w = Widget.create(wid, var, spec, w_cnfig)
    if theme := theme_widget_map.get(w_type):
        lv_add(CallExpression(theme, w.obj))
    await set_obj_properties(w, w_cnfig)
    await add_widgets(w, w_cnfig)
    await spec.to_code(w, w_cnfig)


lv_scr_act_spec = LvScrActType()
lv_scr_act = Widget.create(None, literal("lv_scr_act()"), lv_scr_act_spec, {})
