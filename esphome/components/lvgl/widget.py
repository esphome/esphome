import sys
from typing import Any

from esphome.config_validation import Invalid
from esphome.core import ID, TimePeriod
from esphome.coroutine import FakeAwaitable
from esphome.cpp_generator import MockObjClass

from . import types as ty
from .defines import BTNMATRIX_CTRLS, CONF_ARC, CONF_SPINBOX
from .helpers import get_line_marks
from .types import lv_obj_t

EVENT_LAMB = "event_lamb__"


class WidgetType:
    """
    Describes a type of Widget, e.g. "bar" or "line"
    """

    def __init__(self, name, schema=None, modify_schema=None):
        """
        :param name: The widget name, e.g. "bar"
        :param schema: The config schema for defining a widget
        :param modify_schema: A schema to update the widget
        """
        self.name = name
        self.schema = schema or {}
        if modify_schema is None:
            self.modify_schema = schema
        else:
            self.modify_schema = modify_schema

    @property
    def animated(self):
        return False

    @property
    def w_type(self):
        """
        Get the type associated with this widget
        :return:
        """
        return lv_obj_t

    async def to_code(self, w, config: dict):
        """
        Generate code for a given widget
        :param w: The widget
        :param config: Its configuration
        :return: Generated code as a list of text lines
        """
        raise NotImplementedError(f"No to_code defined for {self.name}")

    def obj_creator(self, parent: MockObjClass, config: dict):
        """
        Create an instance of the widget type
        :param parent: The parent to which it should be attached
        :param config:  Its configuration
        :return: Generated code as a single text line
        """
        return f"lv_{self.name}_create({parent})"

    def get_uses(self):
        """
        Get a list of other widgets used by this one
        :return:
        """
        return ()


class LvScrActType(WidgetType):
    def __init__(self):
        super().__init__("lv_scr_act()")

    def obj_creator(self, parent: MockObjClass, config: dict):
        return []

    async def to_code(self, w, config: dict):
        return []


class Widget:
    def __init__(
        self, var, wtype: WidgetType, config: dict = None, obj=None, parent=None
    ):
        self.var = var
        self.type = wtype
        self.config = config
        self.obj = obj or var
        self.parent = None
        self.scale = 1.0
        self.step = 1.0
        self.range_from = -sys.maxsize
        self.range_to = sys.maxsize
        self.parent = parent

    @staticmethod
    def create(
        name, var, wtype: WidgetType, config: dict = None, obj=None, parent=None
    ):
        w = Widget(var, wtype, config, obj, parent)
        if name is not None:
            widget_map[name] = w
        return w

    def check_null(self):
        return f"if ({self.obj} == nullptr) return"

    def add_state(self, state):
        return [f"lv_obj_add_state({self.obj}, {state})"]

    def clear_state(self, state):
        return [f"lv_obj_clear_state({self.obj}, {state})"]

    def set_state(self, state, cond):
        return [
            f" if({cond}) lv_obj_add_state({self.obj}, {state}); else lv_obj_clear_state({self.obj}, {state})"
        ]

    def has_state(self, state):
        """
        Return code that checks for a given state
        :param state: A state bit
        :return:
        """
        return f"(lv_obj_get_state({self.obj}) & ({state})) != 0"

    def is_pressed(self):
        return self.has_state("LV_STATE_PRESSED")

    def is_checked(self):
        return self.has_state("LV_STATE_CHECKED")

    def add_flag(self, flag):
        return [f"lv_obj_add_flag({self.obj}, {flag})"]

    def clear_flag(self, flag):
        return [f"lv_obj_clear_flag({self.obj}, {flag})"]

    def set_property(self, prop, value, animated: bool = None, ltype=None):
        if isinstance(value, dict):
            value = value.get(prop)
        if value is None:
            return []
        init = get_line_marks(value)
        if animated is None or self.type.animated is not True:
            animated = ""
        else:
            animated = f""", {"LV_ANIM_ON" if animated else "LV_ANIM_OFF"}"""
        if isinstance(value, TimePeriod):
            value = value.total_milliseconds
        ltype = ltype or self.__type_base()
        init.append(f"lv_{ltype}_set_{prop}({self.obj}, {value} {animated})")
        return init

    def get_property(self, prop, ltype=None):
        ltype = ltype or self.__type_base()
        return [f"lv_{ltype}_get_{prop}({self.obj})"]

    def set_style(self, prop, value, state):
        if value is None:
            return []
        if isinstance(value, list):
            value = "|".join(value)
        return [f"lv_obj_set_style_{prop}({self.obj}, {value}, {state})"]

    def set_event_cb(self, code, *varargs):
        init = add_temp_var("event_callback_t", EVENT_LAMB)
        init.extend([f"{EVENT_LAMB} = [](lv_event_t *event_data) {{ {code} ;}} \n"])
        for arg in varargs:
            init.extend(
                [
                    f"lv_obj_add_event_cb({self.obj}, {EVENT_LAMB}, {arg}, nullptr)",
                ]
            )
        return init

    def get_number_value(self):
        if self.scale == 1.0:
            return f"lv_{self.__type_base()}_get_value({self.obj})"
        return f"lv_{self.__type_base()}_get_value({self.obj})/{self.scale:#f}f"

    def get_value(self):
        if isinstance(self.type.w_type, ty.LvType):
            return self.type.w_type.value(self)
        return self.obj

    def get_args(self):
        if isinstance(self.type.w_type, ty.LvType):
            return self.type.w_type.args
        return [(ty.lv_obj_t_ptr, "obj")]

    def set_value(self, value, animated: bool = None):
        if animated is None or self.__type_base() in (CONF_ARC, CONF_SPINBOX):
            animated = ""
        else:
            animated = f""", {"LV_ANIM_ON" if animated else "LV_ANIM_OFF"}"""
        if self.scale != 1.0:
            value = f"{value} * {self.scale:#f}"
        return [f"lv_{self.__type_base()}_set_value({self.obj}, {value} {animated})"]

    def get_mxx_value(self, which: str):
        if self.scale == 1.0:
            mult = ""
        else:
            mult = f"/ {self.scale:#f}"
        if self.__type_base() == CONF_SPINBOX and which in ("max", "min"):
            gval = f"((lv_spinbox_t *){self.obj})->range_{which}"
        else:
            gval = f"lv_{self.__type_base()}_get_{which}_value({self.obj})"
        return f"({gval} {mult})"

    def __type_base(self):
        wtype = self.type.w_type
        base = str(wtype)
        if base.startswith("Lv"):
            return f"{wtype}".removeprefix("Lv").removesuffix("Type").lower()
        return f"{wtype}".removeprefix("lv_").removesuffix("_t")

    def __str__(self):
        return f"({self.var}, {self.type})"


class MatrixButton(Widget):
    """
    Describes a button within a button matrix.
    """

    @staticmethod
    def create_button(name, var, wtype: WidgetType, config: dict, index):
        w = MatrixButton(var, wtype, config, index)
        if name is not None:
            widget_map[name] = w
        return w

    def __init__(self, btnm, btype, config, index):
        super().__init__(btnm, btype, config, obj=btnm.obj)
        self.index = index

    def get_value(self):
        return self.is_checked()

    def get_obj(self):
        return self.var.obj

    def check_null(self):
        return None

    @staticmethod
    def map_ctrls(ctrls):
        clist = []
        for item in ctrls:
            item = item.upper().removeprefix("LV_BTNMATRIX_CTRL_")
            assert item in BTNMATRIX_CTRLS
            clist.append(f"(int)LV_BTNMATRIX_CTRL_{item}")
        return "|".join(clist)

    def set_ctrls(self, *ctrls):
        return [
            f"lv_btnmatrix_set_btn_ctrl({self.var.obj}, {self.index}, {self.map_ctrls(ctrls)})"
        ]

    def clear_ctrls(self, *ctrls):
        return [
            f"lv_btnmatrix_clear_btn_ctrl({self.var.obj}, {self.index}, {self.map_ctrls(ctrls)})"
        ]

    def add_flag(self, flag):
        if flag.endswith("CLICKABLE"):
            return self.var.add_flag(flag)
        flag = flag.removeprefix("LV_OBJ_FLAG_").upper()
        return self.set_ctrls(flag)

    def clear_flag(self, flag):
        return self.clear_ctrls(flag.removeprefix("LV_OBJ_FLAG_"))

    def add_state(self, state):
        return self.set_ctrls(state.removeprefix("LV_STATE_"))

    def clear_state(self, state):
        return self.clear_ctrls(state.removeprefix("LV_STATE_"))

    def set_state(self, state, cond):
        state = self.map_ctrls([state.removeprefix("LV_STATE_")])
        return [
            f"""if({cond}) lv_btnmatrix_set_btn_ctrl({self.var.obj}, {self.index}, {state}); else
        lv_btnmatrix_clear_btn_ctrl({self.var.obj}, {self.index}, {state})"""
        ]

    def index_check(self):
        return f"(lv_btnmatrix_get_selected_btn({self.var.obj}) == {self.index})"

    def has_state(self, state):
        state = self.map_ctrls([state.upper().removeprefix("LV_STATE_")])
        return f"(lv_btnmatrix_has_btn_ctrl({self.var.obj}, {self.index}, {state}))"

    def is_pressed(self):
        return f'({self.index_check()} && {self.var.has_state("LV_STATE_PRESSED")})'

    def is_checked(self):
        return f'({self.index_check()} && {self.has_state("LV_STATE_CHECKED")})'

    def set_event_cb(self, code, *varargs):
        init = add_temp_var("event_callback_t", EVENT_LAMB)
        init.append(
            f"""{EVENT_LAMB} = [](lv_event_t *e) {{
                    if {self.index_check()} {{
                        {code};
                    }}
                }}"""
        )
        for arg in varargs:
            init.append(
                f"lv_obj_add_event_cb({self.var.obj}, {EVENT_LAMB}, {arg}, nullptr)"
            )
        return init

    def set_selected(self):
        return [f"lv_btnmatrix_set_selected_btn({self.var.obj}, {self.index})"]

    def set_width(self, width):
        return [f"lv_btnmatrix_set_btn_width({self.var.obj}, {self.index}, {width})"]


lv_temp_vars = set()  # Temporary variables


def add_temp_var(var_type, var_name):
    if var_name in lv_temp_vars:
        return []
    lv_temp_vars.add(var_name)
    return [f"{var_type} * {var_name}"]


theme_widget_map = {}
# Map of widgets to their config, used for trigger generation
widget_map: dict[Any, Widget] = {}
widgets_completed = False  # will be set true when all widgets are available


def get_widget_generator(wid):
    while True:
        if obj := widget_map.get(wid):
            return obj
        if widgets_completed:
            raise Invalid(
                f"Widget {wid} not found, yet all widgets should be defined by now"
            )
        yield


async def get_widget(wid: ID) -> Widget:
    if obj := widget_map.get(wid):
        return obj
    return await FakeAwaitable(get_widget_generator(wid))
