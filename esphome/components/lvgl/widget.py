import sys
from typing import Any, Union

from esphome import codegen as cg, config_validation as cv
from esphome.config_validation import Invalid
from esphome.const import CONF_GROUP, CONF_ID, CONF_STATE, CONF_TYPE
from esphome.core import CORE, ID, TimePeriod
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    VariableDeclarationExpression,
)

from .defines import (
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
    CONF_WIDGETS,
    OBJ_FLAGS,
    PARTS,
    STATES,
    TYPE_FLEX,
    TYPE_GRID,
    ConstantLiteral,
    LValidator,
    join_enums,
    literal,
)
from .helpers import add_lv_use
from .lvcode import add_line_marks, lv, lv_add, lv_assign, lv_expr, lv_obj
from .schemas import ALL_STYLES, STYLE_REMAP, WIDGET_TYPES
from .types import LvType, WidgetType, lv_coord_t, lv_group_t, lv_obj_t, lv_obj_t_ptr

EVENT_LAMB = "event_lamb__"


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

    @staticmethod
    def create(name, var, wtype: WidgetType, config: dict = None):
        w = Widget(var, wtype, config)
        if name is not None:
            widget_map[name] = w
        return w

    @property
    def obj(self):
        if self.type.is_compound():
            return MockObj(f"{self.var}->obj")
        return self.var

    def add_state(self, state):
        return lv_obj.add_state(self.obj, literal(state))

    def clear_state(self, state):
        return lv_obj.clear_state(self.obj, literal(state))

    def has_state(self, state):
        return lv_expr.obj_get_state(self.obj) & literal(state) != 0

    def add_flag(self, flag):
        return lv_obj.add_flag(self.obj, literal(flag))

    def clear_flag(self, flag):
        return lv_obj.clear_flag(self.obj, literal(flag))

    def set_property(self, prop, value, animated: bool = None):
        if isinstance(value, dict):
            value = value.get(prop)
        if value is None:
            return
        if isinstance(value, TimePeriod):
            value = value.total_milliseconds
        if animated is None or self.type.animated is not True:
            lv.call(f"{self.type.lv_name}_set_{prop}", self.obj, value)
        else:
            lv.call(
                f"{self.type.lv_name}_set_{prop}",
                self.obj,
                value,
                "LV_ANIM_ON" if animated else "LV_ANIM_OFF",
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
        return value * self.scale

    def is_selected(self):
        """
        Overridable property to determine if the widget is selected. Will be None except
        for matrix buttons
        :return:
        """
        return None


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
    if layout := config.get(CONF_LAYOUT):
        layout_type: str = layout[CONF_TYPE]
        lv_obj.set_layout(w.obj, literal(f"LV_LAYOUT_{layout_type.upper()}"))
        if layout_type == TYPE_GRID:
            wid = config[CONF_ID]
            rows = "{" + ",".join(layout[CONF_GRID_ROWS]) + ", LV_GRID_TEMPLATE_LAST}"
            row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=lv_coord_t)
            row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
            w.set_style("grid_row_dsc_array", row_array, 0)
            columns = (
                "{" + ",".join(layout[CONF_GRID_COLUMNS]) + ", LV_GRID_TEMPLATE_LAST}"
            )
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
            lv_obj.set_flex_flow(
                w.obj, literal(f"LV_FLEX_FLOW_{layout[CONF_FLEX_FLOW]}")
            )
            main = layout[CONF_FLEX_ALIGN_MAIN]
            cross = layout[CONF_FLEX_ALIGN_CROSS]
            track = layout[CONF_FLEX_ALIGN_TRACK]
            lv_obj.set_flex_align(w.obj, main, cross, track)
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
                prop_r = STYLE_REMAP.get(prop, prop)
                w.set_style(prop_r, value, lv_state)
    if group := add_group(config.get(CONF_GROUP)):
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
            state = f"LV_STATE_{key.upper}"
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
        var = MockObj(wid, "->")
        decl = VariableDeclarationExpression(lv_obj_t, "*", wid)
        CORE.add_global(decl)
        CORE.register_variable(wid, var)
        lv_assign(var, creator)

    widget = Widget.create(wid, var, spec, w_cnfig)
    await set_obj_properties(widget, w_cnfig)
    await add_widgets(widget, w_cnfig)
    await spec.to_code(widget, w_cnfig)


lv_scr_act_spec = LvScrActType()
lv_scr_act = Widget.create(None, ConstantLiteral("lv_scr_act()"), lv_scr_act_spec, {})

lv_groups = {}  # Widget group names


def add_group(name):
    if name is None:
        return None
    fullname = f"lv_esp_group_{name}"
    if name not in lv_groups:
        gid = ID(fullname, True, type=lv_group_t.operator("ptr"))
        lv_add(
            AssignmentExpression(
                type_=gid.type, modifier="", name=fullname, rhs=lv_expr.group_create()
            )
        )
        lv_groups[name] = ConstantLiteral(fullname)
    return lv_groups[name]
