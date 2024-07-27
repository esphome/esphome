from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WIDTH
from esphome.core import ID

from ...cpp_generator import MockObjClass
from .codegen import action_to_code
from .defines import (
    BTNMATRIX_CTRLS,
    CONF_BUTTONMATRIX,
    CONF_BUTTONS,
    CONF_CONTROL,
    CONF_KEY_CODE,
    CONF_ONE_CHECKED,
    CONF_ROWS,
    CONF_SELECTED,
    CONF_TEXT,
)
from .lv_validation import key_code, lv_bool
from .schemas import automation_schema
from .types import LvBtnmBtn, ObjUpdateAction, char_ptr, lv_btn_t, lv_btnmatrix_t
from .widget import MatrixButton, Widget, WidgetType, get_widget

BTNM_BTN_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TEXT): cv.string,
        cv.Optional(CONF_KEY_CODE): key_code,
        cv.GenerateID(): cv.declare_id(LvBtnmBtn),
        cv.Optional(CONF_WIDTH, default=1): cv.positive_int,
        cv.Optional(CONF_CONTROL): cv.ensure_list(
            cv.Schema({cv.Optional(k.lower()): cv.boolean for k in BTNMATRIX_CTRLS})
        ),
    }
).extend(automation_schema(lv_btn_t))

BTNMATRIX_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ONE_CHECKED, default=False): lv_bool,
        cv.Required(CONF_ROWS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
                }
            )
        ),
    }
)


class BtnmatrixBtnType(WidgetType):
    def __init__(self):
        super().__init__("btnmatrix_btn", {}, {})

    @property
    def w_type(self):
        return LvBtnmBtn

    async def to_code(self, w, config: dict):
        return []


btn_btn_spec = BtnmatrixBtnType()


async def get_button_data(config, text_id, btnm: Widget):
    """
    Process a button matrix button list
    :param config: The row list
    :param text_id: An id basis for the text array
    :param btnm: The parent variable
    :return: text array id, control list, width list
    """
    text_list = []
    ctrl_list = []
    width_list = []
    key_list = []
    btn_id_list = []
    for row in config:
        for btnconf in row.get(CONF_BUTTONS) or ():
            bid = btnconf[CONF_ID]
            index = len(width_list)
            MatrixButton.create_button(bid, btnm, btn_btn_spec, btnconf, index)
            btn_id_list.append(cg.new_Pvariable(bid, index))
            if text := btnconf.get(CONF_TEXT):
                text_list.append(f"{cg.safe_exp(text)}")
            else:
                text_list.append("")
            key_list.append(btnconf.get(CONF_KEY_CODE) or 0)
            width_list.append(btnconf[CONF_WIDTH])
            ctrl = ["(int)LV_BTNMATRIX_CTRL_CLICK_TRIG"]
            if controls := btnconf.get(CONF_CONTROL):
                for item in controls:
                    ctrl.extend(
                        [
                            f"(int)LV_BTNMATRIX_CTRL_{k.upper()}"
                            for k, v in item.items()
                            if v
                        ]
                    )
            ctrl_list.append("|".join(ctrl))
        text_list.append('"\\n"')
    text_list = text_list[:-1]
    text_list.append("NULL")
    text_id = ID(f"{text_id.id}_text_array", is_declaration=True, type=char_ptr)
    text_id = cg.static_const_array(
        text_id, cg.RawExpression("{" + ",".join(text_list) + "}")
    )
    return text_id, ctrl_list, width_list, key_list, btn_id_list


def set_btn_data(btnm: Widget, ctrl_list, width_list):
    init = []
    for index, ctrl in enumerate(ctrl_list):
        init.append(f"lv_btnmatrix_set_btn_ctrl({btnm.obj}, {index}, {ctrl})")
    for index, width in enumerate(width_list):
        init.append(f"lv_btnmatrix_set_btn_width({btnm.obj}, {index}, {width})")
    return init


class BtnmatrixType(WidgetType):
    def __init__(self):
        super().__init__(CONF_BUTTONMATRIX, BTNMATRIX_SCHEMA, {})

    @property
    def w_type(self):
        return lv_btnmatrix_t

    async def to_code(self, w: Widget, config):
        if CONF_ROWS not in config:
            return []
        cid = config[CONF_ID]
        text_id, ctrl_list, width_list, key_list, btn_id_list = await get_button_data(
            config[CONF_ROWS], cid, w
        )
        init = [f"lv_btnmatrix_set_map({w.obj}, {text_id})"]
        init.extend(set_btn_data(w, ctrl_list, width_list))
        init.append(
            f"lv_btnmatrix_set_one_checked({w.obj}, {config[CONF_ONE_CHECKED]})"
        )
        for index, key in enumerate(key_list):
            if key != 0:
                init.append(f"{w.var}->set_key({index}, {key})")
        for bid in btn_id_list:
            init.append(f"{w.var}->add_btn({bid})")
        return init

    def get_uses(self):
        return ("btnmatrix",)

    def obj_creator(self, parent: MockObjClass, config: dict):
        return f"lv_btnmatrix_create({parent})"


btnmatrix_spec = BtnmatrixType()


@automation.register_action(
    "lvgl.matrixbutton.update",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_WIDTH): cv.positive_int,
            cv.Optional(CONF_CONTROL): cv.ensure_list(
                cv.Schema(
                    {cv.Optional(k.lower()): cv.boolean for k in BTNMATRIX_CTRLS}
                ),
            ),
            cv.Required(CONF_ID): cv.use_id(LvBtnmBtn),
            cv.Optional(CONF_SELECTED): lv_bool,
        }
    ),
)
async def button_update_to_code(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    assert isinstance(widget, MatrixButton)
    init = []
    if (width := config.get(CONF_WIDTH)) is not None:
        init.extend(widget.set_width(width))
    if config.get(CONF_SELECTED):
        init.extend(widget.set_selected())
    if controls := config.get(CONF_CONTROL):
        adds = []
        clrs = []
        for item in controls:
            adds.extend([k for k, v in item.items() if v])
            clrs.extend([k for k, v in item.items() if not v])
        if adds:
            init.extend(widget.set_ctrls(*adds))
        if clrs:
            init.extend(widget.clear_ctrls(*clrs))
    return await action_to_code(init, action_id, widget.var, template_arg, args)
