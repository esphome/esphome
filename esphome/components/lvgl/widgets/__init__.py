import sys

from esphome import codegen as cg, config_validation as cv
from esphome.automation import register_action
from esphome.config_validation import Invalid, Schema
from esphome.const import (
    CONF_DEFAULT,
    CONF_GROUP,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STATE,
    CONF_TYPE,
)
from esphome.core import ID, EsphomeError, TimePeriod
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import MockObj
from esphome.types import Expression

from ..defines import (
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
    CONF_PAD_COLUMN,
    CONF_PAD_ROW,
    CONF_SCALE,
    CONF_STYLES,
    CONF_WIDGETS,
    OBJ_FLAGS,
    PARTS,
    STATES,
    TYPE_FLEX,
    TYPE_GRID,
    LValidator,
    add_lv_use,
    call_lambda,
    get_styles_used,
    get_theme_widget_map,
    get_widget_map,
    get_widgets_completed,
    join_enums,
    literal,
)
from ..lv_validation import lv_int
from ..lvcode import (
    LvConditional,
    add_line_marks,
    lv,
    lv_add,
    lv_assign,
    lv_expr,
    lv_obj,
    lv_Pvariable,
    lvgl_static,
)
from ..types import (
    LV_STATE,
    LvCompound,
    LvType,
    ObjUpdateAction,
    lv_coord_t,
    lv_obj_t,
    lv_obj_t_ptr,
)

EVENT_LAMB = "event_lamb__"


class WidgetType:
    """
    Describes a type of Widget, e.g. "bar" or "line"
    """

    def __init__(
        self,
        name: str,
        w_type: LvType,
        parts: tuple,
        schema=None,
        modify_schema=None,
        lv_name=None,
        is_mock: bool = False,
    ):
        """
        :param name: The widget name, e.g. "bar"
        :param w_type: The C type of the widget
        :param parts: What parts this widget supports
        :param schema: The config schema for defining a widget
        :param modify_schema: A schema to update the widget, defaults to the same as the schema
        :param lv_name: The name of the LVGL widget in the LVGL library, if different from the name
        :param is_mock: Whether this widget is a mock widget, i.e. not a real LVGL widget
        """
        self.name = name
        self.lv_name = lv_name or name
        self.w_type = w_type
        self.parts = parts
        if not isinstance(schema, Schema):
            schema = Schema(schema or {})
        self.schema = schema
        if modify_schema is None:
            modify_schema = schema
        if not isinstance(modify_schema, Schema):
            modify_schema = Schema(modify_schema)
        self.modify_schema = modify_schema
        self.mock_obj = MockObj(f"lv_{self.lv_name}", "_")

        # Local import to avoid circular import
        from ..automation import update_to_code
        from ..schemas import WIDGET_TYPES, base_update_schema

        if not is_mock:
            if self.name in WIDGET_TYPES:
                raise EsphomeError(f"Duplicate definition of widget type '{self.name}'")
            WIDGET_TYPES[self.name] = self

            # Register the update action automatically, adding widget-specific properties
            register_action(
                f"lvgl.{self.name}.update",
                ObjUpdateAction,
                base_update_schema(self, self.parts).extend(self.modify_schema),
                synchronous=True,
            )(update_to_code)

    @property
    def animated(self):
        return False

    @property
    def required_component(self):
        return None

    def is_compound(self):
        return self.w_type.inherits_from(LvCompound)

    async def create_to_code(self, config: dict, parent: MockObj) -> "Widget":
        """
        Generate code for a widget creation.
        :param config: The configuration for the widget
        :param parent: The parent to which it should be attached
        """

        creator = await self.obj_creator(parent, config)
        add_lv_use(self.name)
        add_lv_use(*self.get_uses())
        wid = config[CONF_ID]
        add_line_marks(wid)
        if self.is_compound():
            var = cg.new_Pvariable(wid)
            lv_add(var.set_obj(creator))
            await self.on_create(var.obj, config)
        else:
            var = lv_Pvariable(lv_obj_t, wid)
            lv_assign(var, creator)
            await self.on_create(var, config)

        w = Widget.create(wid, var, self, config)
        if theme := get_theme_widget_map().get(self.name):
            for part, states in theme.items():
                part = "LV_PART_" + part.upper()
                for state, style in states.items():
                    state = "LV_STATE_" + state.upper()
                    if state == "LV_STATE_DEFAULT":
                        lv_state = literal(part)
                    elif part == "LV_PART_MAIN":
                        lv_state = literal(state)
                    else:
                        lv_state = join_enums((state, part))
                    w.add_style(style, lv_state)
        await set_obj_properties(w, config)
        await add_widgets(w, config)
        await self.to_code(w, config)
        return w

    async def to_code(self, w: "Widget", config: dict):
        """
        Update a widget, also called when creating
        :param config:
        :return:
        """

    async def obj_creator(self, parent: MockObj, config: dict):
        """
        Create an instance of the widget type
        :param parent: The parent to which it should be attached
        :param config:  Its configuration
        :return: Generated code as a single text line
        """
        return lv_expr.call(f"{self.lv_name}_create", parent)

    async def on_create(self, var: MockObj, config: dict):
        """
        Called from to_code when the widget is created, to set up any initial properties
        :param var: The variable representing the widget
        :param config: Its configuration
        """

    def get_uses(self):
        """
        Get a list of other widgets used by this one
        :return:
        """
        return ()

    async def get_max(self, config: dict):
        return sys.maxsize

    async def get_min(self, config: dict):
        return -sys.maxsize

    def get_step(self, config: dict):
        return 1

    def get_scale(self, config: dict):
        return 1.0

    def validate(self, value):
        """
        Provides an opportunity for custom validation for a given widget type
        :param value:
        :return:
        """
        return value

    def final_validate(self, widget, update_config, widget_config, path):
        """
        Allow final validation for a given widget type update action
        :param widget: A widget
        :param update_config: The configuration for the update action
        :param widget_config: The configuration for the widget itself
        :param path: The path to the widget, for error reporting
        """


class Widget:
    """
    Represents a Widget.
    This class has a lot of methods. Adding any more runs foul of lint checks ("too many public methods").
    """

    def __init__(self, var, wtype: WidgetType, config: dict = None):
        self.var = var
        self.type = wtype
        self.config = config
        self.scale = 1.0
        self.range_from = -sys.maxsize
        self.range_to = sys.maxsize
        if wtype.is_compound():
            self.obj = MockObj(f"{self.var}->obj")
        else:
            self.obj = var
        self.outer = None
        self.move_to_foreground = False
        # Properties for linear equations
        self.slope = None
        self.y_int = None

    @staticmethod
    def create(name, var, wtype: WidgetType, config: dict = None):
        w = Widget(var, wtype, config)
        get_widget_map()[name] = w
        return w

    def set_state(self, state: MockObj, value: bool | Expression):
        lv_add(lvgl_static.lv_obj_set_state_value(self.obj, state, value))

    def has_state(self, state: MockObj):
        return lv_expr.obj_has_state(self.obj, state)

    def is_pressed(self):
        return self.has_state(LV_STATE.PRESSED)

    def is_checked(self):
        return self.has_state(LV_STATE.CHECKED)

    def add_flag(self, flag):
        if "|" in flag:
            flag = f"(lv_obj_flag_t)({flag})"
        return lv_obj.add_flag(self.obj, literal(flag))

    def clear_flag(self, flag):
        if "|" in flag:
            flag = f"(lv_obj_flag_t)({flag})"
        return lv_obj.remove_flag(self.obj, literal(flag))

    def add_style(self, style_id, state=LV_STATE.DEFAULT):
        if "|" in state:
            state = f"(lv_state_t)({state})"
        lv_obj.add_style(self.obj, MockObj(style_id), literal(state))

    async def set_property(
        self, prop, value, animated: bool = None, lv_name=None, processor=None
    ):
        """
        Set a property of the widget.
        :param prop:  The property name
        :param value:  The value
        :param animated:  If the change should be animated
        :param lv_name:  The base type of the widget e.g. "obj"
        """

        from ..schemas import ALL_STYLES, remap_property

        if isinstance(value, dict):
            value = value.get(prop)
            if value is None:
                return
            if not processor and isinstance(ALL_STYLES.get(prop), LValidator):
                processor = ALL_STYLES[prop]
            if isinstance(processor, LValidator):
                processor = processor.process
            if processor:
                value = await processor(value)
        elif value is None:
            return
        prop = remap_property(prop)
        if isinstance(value, TimePeriod):
            value = value.total_milliseconds
        elif isinstance(value, str):
            value = literal(value)
        elif isinstance(value, ID):
            value = MockObj(value)
        lv_name = lv_name or self.type.lv_name
        if animated is None or self.type.animated is not True:
            lv.call(f"{lv_name}_set_{prop}", self.obj, value)
        else:
            lv.call(
                f"{lv_name}_set_{prop}",
                self.obj,
                value,
                literal("LV_ANIM_ON" if animated else "LV_ANIM_OFF"),
            )

    def get_property(self, prop, ltype=None):
        ltype = ltype or self.__type_base()
        return cg.RawExpression(f"lv_{ltype}_get_{prop}({self.obj})")

    def set_style(self, prop: str, value, state=LV_STATE.DEFAULT):
        if value is None:
            return
        get_styles_used().add(prop)
        if isinstance(value, str):
            value = literal(value)
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
            return self.type.w_type.args.copy()
        return [(lv_obj_t_ptr, "obj")]

    def get_value(self):
        if isinstance(self.type.w_type, LvType):
            result = self.type.w_type.value(self)
            if isinstance(result, list):
                return result[0]
            return result
        return self.obj

    def get_values(self):
        if isinstance(self.type.w_type, LvType):
            result = self.type.w_type.value(self)
            if isinstance(result, list):
                return result
            return [result]
        return [self.obj]

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
        return

    def get_scale(self):
        return self.type.get_scale(self.config)


class LvScrActType(WidgetType):
    """
    A "widget" representing the active screen.
    """

    def __init__(self):
        super().__init__("lv_screen_active()", lv_obj_t, (), is_mock=True)

    async def to_code(self, w, config: dict):
        pass


def get_screen_active(lv_comp: MockObj) -> Widget:
    return Widget(lv_comp.get_screen_active(), LvScrActType(), {})


def get_widget_generator(wid):
    """
    Used to wait for a widget during code generation.
    :param wid:
    :return:
    """
    widget_map = get_widget_map()
    while True:
        if obj := widget_map.get(wid):
            return obj
        if get_widgets_completed():
            raise Invalid(
                f"Widget {wid} not found, yet all widgets should be defined by now"
            )
        yield


async def get_widget_(wid):
    if obj := get_widget_map().get(wid):
        return obj
    return await FakeAwaitable(get_widget_generator(wid))


def widgets_wait_generator():
    while True:
        if get_widgets_completed():
            return
        yield


async def wait_for_widgets():
    if get_widgets_completed():
        return
    await FakeAwaitable(widgets_wait_generator())


async def get_widgets(config: dict | list, id: str = CONF_ID) -> list[Widget]:
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

    from ..schemas import ALL_STYLES

    props = {}
    for prop in [*ALL_STYLES, *OBJ_FLAGS, CONF_STYLES, CONF_GROUP]:
        if prop in config:
            if prop == CONF_SCALE:
                props[CONF_SCALE + "_x"] = config[prop]
                props[CONF_SCALE + "_y"] = config[prop]
            else:
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


def _size_to_str(value):
    if isinstance(value, float):
        return f"lv_pct({int(value * 100)})"
    return str(value)


async def set_obj_properties(w: Widget, config):
    """Generate a list of C++ statements to apply properties to an lv_obj_t"""

    from ..schemas import ALL_STYLES, OBJ_PROPERTIES, remap_property

    if layout := config.get(CONF_LAYOUT):
        layout_type: str = layout[CONF_TYPE]
        add_lv_use(layout_type)
        lv_obj.set_layout(w.obj, literal(f"LV_LAYOUT_{layout_type.upper()}"))
        if (pad_row := layout.get(CONF_PAD_ROW)) is not None:
            w.set_style(CONF_PAD_ROW, pad_row)
        if (pad_column := layout.get(CONF_PAD_COLUMN)) is not None:
            w.set_style(CONF_PAD_COLUMN, pad_column)
        if layout_type == TYPE_GRID:
            wid = config[CONF_ID]
            rows = [_size_to_str(x) for x in layout[CONF_GRID_ROWS]]
            rows = "{" + ",".join(rows) + ", LV_GRID_TEMPLATE_LAST}"
            row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=lv_coord_t)
            row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
            w.set_style("grid_row_dsc_array", row_array)
            columns = [_size_to_str(x) for x in layout[CONF_GRID_COLUMNS]]
            columns = "{" + ",".join(columns) + ", LV_GRID_TEMPLATE_LAST}"
            column_id = ID(f"{wid}_column_dsc", is_declaration=True, type=lv_coord_t)
            column_array = cg.static_const_array(column_id, cg.RawExpression(columns))
            w.set_style("grid_column_dsc_array", column_array)
            w.set_style(
                CONF_GRID_COLUMN_ALIGN, literal(layout.get(CONF_GRID_COLUMN_ALIGN))
            )
            w.set_style(CONF_GRID_ROW_ALIGN, literal(layout.get(CONF_GRID_ROW_ALIGN)))
        if layout_type == TYPE_FLEX:
            lv_obj.set_flex_flow(w.obj, literal(layout[CONF_FLEX_FLOW]))
            main = literal(layout[CONF_FLEX_ALIGN_MAIN])
            cross = layout[CONF_FLEX_ALIGN_CROSS]
            if cross == "LV_FLEX_ALIGN_STRETCH":
                cross = "LV_FLEX_ALIGN_CENTER"
            cross = literal(cross)
            track = literal(layout[CONF_FLEX_ALIGN_TRACK])
            lv_obj.set_flex_align(w.obj, main, cross, track)
    parts = collect_parts(config)
    for part, states in parts.items():
        part = "LV_PART_" + part.upper()
        for state, props in states.items():
            state = "LV_STATE_" + state.upper()
            if state == "LV_STATE_DEFAULT":
                lv_state = literal(part)
            elif part == "LV_PART_MAIN":
                lv_state = literal(state)
            else:
                lv_state = join_enums((state, part))
            for style_id in props.get(CONF_STYLES, ()):
                w.add_style(style_id, lv_state)
            for prop, value in {
                k: v for k, v in props.items() if k in ALL_STYLES
            }.items():
                if isinstance(ALL_STYLES[prop], LValidator):
                    value = await ALL_STYLES[prop].process(value)
                prop_r = remap_property(prop)
                w.set_style(prop_r, value, lv_state)
    if group := config.get(CONF_GROUP):
        group = await cg.get_variable(group)
        lv.group_add_obj(group, w.obj)
    props = parts[CONF_MAIN][CONF_DEFAULT]
    lambs = {}
    flag_set = set()
    flag_clr = set()
    for prop, value in {k: v for k, v in props.items() if k in OBJ_FLAGS}.items():
        if isinstance(value, cv.Lambda):
            lambs[prop] = value
        elif value:
            flag_set.add(prop)
        else:
            flag_clr.add(prop)
    if flag_set:
        adds = join_enums(flag_set, "LV_OBJ_FLAG_")
        w.add_flag(adds)
    if flag_clr:
        clrs = join_enums(flag_clr, "LV_OBJ_FLAG_")
        w.clear_flag(clrs)
    for key, value in lambs.items():
        lamb = await cg.process_lambda(value, [], capture="=", return_type=cg.bool_)
        flag = f"LV_OBJ_FLAG_{key.upper()}"
        with LvConditional(call_lambda(lamb)) as cond:
            w.add_flag(flag)
            cond.else_()
            w.clear_flag(flag)

    for key, value in config.get(CONF_STATE, {}).items():
        if isinstance(value, cv.Lambda):
            value = call_lambda(
                await cg.process_lambda(value, [], capture="=", return_type=cg.bool_)
            )
        state = getattr(LV_STATE, key.upper())
        w.set_state(state, value)

    for property in OBJ_PROPERTIES:
        await w.set_property(property, config, lv_name="obj")


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


async def widget_to_code(w_cnfig, w_type: WidgetType | str, parent) -> Widget:
    """
    Converts a Widget definition to C code.
    :param w_cnfig: The widget configuration
    :param w_type:  The Widget type
    :param parent: The parent to which the widget should be added
    :return:
    """

    from ..schemas import WIDGET_TYPES

    spec: WidgetType = (
        w_type if isinstance(w_type, WidgetType) else WIDGET_TYPES[w_type]
    )
    return await spec.create_to_code(w_cnfig, parent)


class NumberType(WidgetType):
    async def get_max(self, config: dict):
        return await lv_int.process(config.get(CONF_MAX_VALUE, 100))

    async def get_min(self, config: dict):
        return await lv_int.process(config.get(CONF_MIN_VALUE, 0))
