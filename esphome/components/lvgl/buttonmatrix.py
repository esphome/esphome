from esphome import automation
import esphome.codegen as cg
from esphome.components.key_provider import KeyProvider
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WIDTH
from esphome.cpp_generator import MockObj

from .automation import action_to_code
from .button import lv_button_t
from .defines import (
    BUTTONMATRIX_CTRLS,
    CONF_BUTTONS,
    CONF_CONTROL,
    CONF_ITEMS,
    CONF_KEY_CODE,
    CONF_MAIN,
    CONF_ONE_CHECKED,
    CONF_ROWS,
    CONF_SELECTED,
    CONF_TEXT,
)
from .helpers import lvgl_components_required
from .lv_validation import key_code, lv_bool
from .lvcode import lv, lv_add, lv_expr
from .schemas import automation_schema
from .types import (
    LV_BTNMATRIX_CTRL,
    LV_STATE,
    LvBoolean,
    LvCompound,
    LvType,
    ObjUpdateAction,
    char_ptr,
    lv_pseudo_button_t,
)
from .widget import Widget, WidgetType, get_widgets, widget_map

CONF_BUTTONMATRIX = "buttonmatrix"
CONF_BUTTON_TEXT_LIST_ID = "button_text_list_id"

LvButtonMatrixButton = LvBoolean(
    str(cg.uint16),
    parents=(lv_pseudo_button_t,),
)
BUTTONMATRIX_BUTTON_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEXT): cv.string,
        cv.Optional(CONF_KEY_CODE): key_code,
        cv.GenerateID(): cv.declare_id(LvButtonMatrixButton),
        cv.Optional(CONF_WIDTH, default=1): cv.positive_int,
        cv.Optional(CONF_CONTROL): cv.ensure_list(
            cv.Schema(
                {cv.Optional(k.lower()): cv.boolean for k in BUTTONMATRIX_CTRLS.choices}
            )
        ),
    }
).extend(automation_schema(lv_button_t))

BUTTONMATRIX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ONE_CHECKED, default=False): lv_bool,
        cv.GenerateID(CONF_BUTTON_TEXT_LIST_ID): cv.declare_id(char_ptr),
        cv.Required(CONF_ROWS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_BUTTONS): cv.ensure_list(
                        BUTTONMATRIX_BUTTON_SCHEMA
                    ),
                }
            )
        ),
    }
)


class ButtonmatrixButtonType(WidgetType):
    """
    A pseudo-widget for the matrix buttons
    """

    def __init__(self):
        super().__init__("btnmatrix_btn", LvButtonMatrixButton, (), {}, {})

    async def to_code(self, w, config: dict):
        return []


btn_btn_spec = ButtonmatrixButtonType()


class MatrixButton(Widget):
    """
    Describes a button within a button matrix.
    """

    @staticmethod
    def create_button(id, parent, config: dict, index):
        w = MatrixButton(id, parent, config, index)
        widget_map[id] = w
        return w

    def __init__(self, id, parent: Widget, config, index):
        super().__init__(id, btn_btn_spec, config)
        self.parent = parent
        self.index = index
        self.obj = parent.obj

    def is_selected(self):
        return self.parent.var.get_selected() == MockObj(self.var)

    @staticmethod
    def map_ctrls(state):
        state = str(state).upper().removeprefix("LV_STATE_")
        assert state in BUTTONMATRIX_CTRLS.choices
        return getattr(LV_BTNMATRIX_CTRL, state)

    def has_state(self, state):
        state = self.map_ctrls(state)
        return lv_expr.btnmatrix_has_btn_ctrl(self.obj, self.index, state)

    def add_state(self, state):
        state = self.map_ctrls(state)
        return lv.btnmatrix_set_btn_ctrl(self.obj, self.index, state)

    def clear_state(self, state):
        state = self.map_ctrls(state)
        return lv.btnmatrix_clear_btn_ctrl(self.obj, self.index, state)

    def is_pressed(self):
        return self.is_selected() & self.parent.has_state(LV_STATE.PRESSED)

    def is_checked(self):
        return self.has_state(LV_STATE.CHECKED)

    def get_value(self):
        return self.is_checked()

    def check_null(self):
        return None


async def get_button_data(config, buttonmatrix: Widget):
    """
    Process a button matrix button list
    :param config: The row list
    :param buttonmatrix: The parent variable
    :return: text array id, control list, width list
    """
    text_list = []
    ctrl_list = []
    width_list = []
    key_list = []
    for row in config:
        for button_conf in row.get(CONF_BUTTONS) or ():
            bid = button_conf[CONF_ID]
            index = len(width_list)
            MatrixButton.create_button(bid, buttonmatrix, button_conf, index)
            cg.new_variable(bid, index)
            text_list.append(button_conf.get(CONF_TEXT) or "")
            key_list.append(button_conf.get(CONF_KEY_CODE) or 0)
            width_list.append(button_conf[CONF_WIDTH])
            ctrl = ["LV_BTNMATRIX_CTRL_CLICK_TRIG"]
            for item in button_conf.get(CONF_CONTROL, ()):
                ctrl.extend([k for k, v in item.items() if v])
            ctrl_list.append(await BUTTONMATRIX_CTRLS.process(ctrl))
        text_list.append("\n")
    text_list = text_list[:-1]
    text_list.append(cg.nullptr)
    return text_list, ctrl_list, width_list, key_list


lv_buttonmatrix_t = LvType(
    "LvButtonMatrixType",
    parents=(KeyProvider, LvCompound),
    largs=[(cg.uint16, "x")],
    lvalue=lambda w: w.var.get_selected(),
)


class ButtonMatrixType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_BUTTONMATRIX,
            lv_buttonmatrix_t,
            (CONF_MAIN, CONF_ITEMS),
            BUTTONMATRIX_SCHEMA,
            {},
            lv_name="btnmatrix",
        )

    async def to_code(self, w: Widget, config):
        lvgl_components_required.add("BUTTONMATRIX")
        if CONF_ROWS not in config:
            return []
        text_list, ctrl_list, width_list, key_list = await get_button_data(
            config[CONF_ROWS], w
        )
        text_id = config[CONF_BUTTON_TEXT_LIST_ID]
        text_id = cg.static_const_array(text_id, text_list)
        lv.btnmatrix_set_map(w.obj, text_id)
        set_btn_data(w.obj, ctrl_list, width_list)
        lv.btnmatrix_set_one_checked(w.obj, config[CONF_ONE_CHECKED])
        for index, key in enumerate(key_list):
            if key != 0:
                lv_add(w.var.set_key(index, key))

    def get_uses(self):
        return ("btnmatrix",)


def set_btn_data(obj, ctrl_list, width_list):
    for index, ctrl in enumerate(ctrl_list):
        lv.btnmatrix_set_btn_ctrl(obj, index, ctrl)
    for index, width in enumerate(width_list):
        lv.btnmatrix_set_btn_width(obj, index, width)


buttonmatrix_spec = ButtonMatrixType()


@automation.register_action(
    "lvgl.matrix.button.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_WIDTH): cv.positive_int,
            cv.Optional(CONF_CONTROL): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Optional(k.lower()): cv.boolean
                        for k in BUTTONMATRIX_CTRLS.choices
                    }
                ),
            ),
            cv.Required(CONF_ID): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_ID): cv.use_id(LvButtonMatrixButton),
                    },
                    key=CONF_ID,
                )
            ),
            cv.Optional(CONF_SELECTED): lv_bool,
        }
    ),
)
async def button_update_to_code(config, action_id, template_arg, args):
    widgets = await get_widgets(config[CONF_ID])
    assert all(isinstance(w, MatrixButton) for w in widgets)

    async def do_button_update(w: MatrixButton):
        if (width := config.get(CONF_WIDTH)) is not None:
            lv.btnmatrix_set_btn_width(w.obj, w.index, width)
        if config.get(CONF_SELECTED):
            lv.btnmatrix_set_selected_btn(w.obj, w.index)
        if controls := config.get(CONF_CONTROL):
            adds = []
            clrs = []
            for item in controls:
                adds.extend(
                    [f"LV_BTNMATRIX_CTRL_{k.upper()}" for k, v in item.items() if v]
                )
                clrs.extend(
                    [f"LV_BTNMATRIX_CTRL_{k.upper()}" for k, v in item.items() if not v]
                )
            if adds:
                lv.btnmatrix_set_btn_ctrl(
                    w.obj, w.index, await BUTTONMATRIX_CTRLS.process(adds)
                )
            if clrs:
                lv.btnmatrix_clear_btn_ctrl(
                    w.obj, w.index, await BUTTONMATRIX_CTRLS.process(clrs)
                )

    return await action_to_code(
        widgets, do_button_update, action_id, template_arg, args
    )
