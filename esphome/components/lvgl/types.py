from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.key_provider import KeyProvider
from esphome.const import CONF_LED, CONF_VALUE
from esphome.core import ID, Lambda

from .defines import (
    CONF_ANIMIMG,
    CONF_ARC,
    CONF_BAR,
    CONF_BTNMATRIX,
    CONF_BUTTON,
    CONF_CHECKBOX,
    CONF_DROPDOWN,
    CONF_IMG,
    CONF_KEYBOARD,
    CONF_LABEL,
    CONF_LINE,
    CONF_MENU,
    CONF_METER,
    CONF_OBJ,
    CONF_PAGE,
    CONF_ROLLER,
    CONF_SLIDER,
    CONF_SPINBOX,
    CONF_SPINNER,
    CONF_SWITCH,
    CONF_TABVIEW,
    CONF_TEXT,
    CONF_TEXTAREA,
    CONF_TILEVIEW,
)

uint16_t_ptr = cg.uint16.operator("ptr")
lvgl_ns = cg.esphome_ns.namespace("lvgl")
char_ptr = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
lv_coord_t = cg.global_ns.namespace("lv_coord_t")
lv_event_code_t = cg.global_ns.enum("lv_event_code_t")
lv_indev_type_t = cg.global_ns.enum("lv_indev_type_t")
LvglComponent = lvgl_ns.class_("LvglComponent", cg.PollingComponent)
LvglComponentPtr = LvglComponent.operator("ptr")
# LvImagePtr = Image_.operator("ptr")
LVTouchListener = lvgl_ns.class_("LVTouchListener")
LVEncoderListener = lvgl_ns.class_("LVEncoderListener")
IdleTrigger = lvgl_ns.class_("IdleTrigger", automation.Trigger.template())
# FontEngine = lvgl_ns.class_("FontEngine")
ObjUpdateAction = lvgl_ns.class_("ObjUpdateAction", automation.Action)
LvglCondition = lvgl_ns.class_("LvglCondition", automation.Condition)
LvglAction = lvgl_ns.class_("LvglAction", automation.Action)
lv_lambda_t = lvgl_ns.class_("LvLambdaType")
lv_indicator_t = lvgl_ns.class_("lv_indicator_t")
lv_lambda_ptr_t = lvgl_ns.class_("LvLambdaType").operator("ptr")
LvCompound = lvgl_ns.class_("LvCompound")
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
lv_font_t = cg.global_ns.class_("lv_font_t")
lv_page_t = cg.global_ns.class_("LvPageType")
lv_point_t = cg.global_ns.struct("lv_point_t")
lv_msgbox_t = cg.global_ns.struct("lv_msgbox_t")
lv_style_t = cg.global_ns.struct("lv_style_t")
lv_color_t = cg.global_ns.struct("lv_color_t")
lv_meter_indicator_t = cg.global_ns.struct("lv_meter_indicator_t")
lv_meter_indicator_t_ptr = lv_meter_indicator_t.operator("ptr")
lv_obj_base_t = cg.global_ns.class_("lv_obj_t", lv_pseudo_button_t)
lv_obj_t_ptr = lv_obj_base_t.operator("ptr")

# this will be populated later, in __init__.py to avoid circular imports.
WIDGET_TYPES: dict = {}


class LvType(cg.MockObjClass):
    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents", ())
        super().__init__(*args, parents=parens + (lv_obj_base_t,))
        self.args = kwargs.pop("largs", [(lv_obj_t_ptr, "obj")])
        self.value = kwargs.pop("lvalue", lambda w: w.obj)
        self.has_on_value = kwargs.pop("has_on_value", False)
        self.value_property = None

    def get_arg_type(self):
        return self.args[0][0] if len(self.args) else None


class LvNumber(LvType):
    def __init__(self, *args):
        super().__init__(
            *args,
            largs=[(cg.float_, "x")],
            lvalue=lambda w: w.get_number_value(),
            has_on_value=True,
        )
        self.value_property = CONF_VALUE


class LvText(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.std_string, "text")],
            lvalue=lambda w: w.get_property("text")[0],
            **kwargs,
        )
        self.value_property = CONF_TEXT


class LvBoolean(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.bool_, "x")],
            lvalue=lambda w: w.is_checked(),
            has_on_value=True,
            **kwargs,
        )


class LvSelect(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.int_, "x")],
            lvalue=lambda w: w.get_property("selected")[0],
            has_on_value=True,
            **kwargs,
        )


lv_obj_t = LvType("lv_obj_t")
lv_dropdown_list_t = LvType("lv_dropdown_list_t")
lv_menu_t = LvType("lv_menu_t")
lv_menu_entry_t = LvType("lv_obj_t")
lv_meter_t = LvType("lv_meter_t")
lv_btn_t = LvBoolean("lv_btn_t")
lv_checkbox_t = LvBoolean("lv_checkbox_t")
lv_line_t = LvType("lv_line_t")
lv_animimg_t = LvType("lv_animimg_t")
lv_tile_t = LvType("lv_tileview_tile_t")
lv_tab_t = LvType("lv_obj_t")
lv_spinbox_t = LvNumber("lv_spinbox_t")
lv_arc_t = LvNumber("lv_arc_t")
lv_bar_t = LvNumber("lv_bar_t")
lv_slider_t = LvNumber("lv_slider_t")
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
lv_canvas_t = LvType("lv_canvas_t")
lv_dropdown_t = LvSelect("lv_dropdown_t")
lv_roller_t = LvSelect("lv_roller_t")
lv_switch_t = LvBoolean("lv_switch_t")
lv_table_t = LvType("lv_table_t")
lv_chart_t = LvType("lv_chart_t")
lv_img_dsc_t = LvType("lv_img_dsc_t")
lv_img_t = LvType("LvImgType", parents=(LvCompound,))
lv_btnmatrix_t = LvType(
    "LvBtnmatrixType",
    parents=(KeyProvider, LvCompound),
    largs=[(uint16_t_ptr, "x")],
    lvalue=lambda w: f"{w.var}->get_selected( )",
)
LvBtnmBtn = LvType(
    str(cg.uint16),
    parents=(lv_pseudo_button_t,),
    largs=[(bool, "x")],
    lvalue=lambda w: f"{w.var}->get_selected( )",
    has_on_value=True,
)
lv_keyboard_t = LvType(
    "LvKeyboardType",
    parents=(KeyProvider, LvCompound),
    largs=[(cg.const_char_ptr, "text")],
    lvalue=lambda w: f"lv_textarea_get_text({w.obj})",
)
lv_label_t = LvText(
    "lv_label_t",
)

lv_led_t = LvType("lv_led_t")
lv_textarea_t = LvText(
    "lv_textarea_t",
    has_on_value=True,
)

lv_tileview_t = LvType(
    "lv_tileview_t",
    largs=[(lv_obj_t_ptr, "tile")],
    lvalue=lambda w: f"lv_tileview_get_tile_act({w.obj})",
)
lv_tabview_t = LvType(
    "lv_tabview_t",
    largs=[(lv_obj_t_ptr, "tab")],
    lvalue=lambda w: f"lv_obj_get_child(lv_tabview_get_content({w.obj}), lv_tabview_get_tab_act({w.obj}))",
)
lv_spinner_t = lv_obj_t
lv_ticks_t = lv_obj_t
lv_tick_style_t = lv_obj_t

LV_TYPES = {
    CONF_ANIMIMG: lv_animimg_t,
    CONF_ARC: lv_arc_t,
    CONF_BUTTON: lv_btn_t,
    # For backwards compatibility
    CONF_BAR: lv_bar_t,
    CONF_BTNMATRIX: lv_btnmatrix_t,
    CONF_CHECKBOX: lv_checkbox_t,
    CONF_DROPDOWN: lv_dropdown_t,
    CONF_IMG: lv_img_t,
    CONF_KEYBOARD: lv_keyboard_t,
    CONF_LABEL: lv_label_t,
    CONF_LED: lv_led_t,
    CONF_LINE: lv_line_t,
    CONF_MENU: lv_menu_t,
    CONF_METER: lv_meter_t,
    CONF_OBJ: lv_obj_t,
    CONF_PAGE: lv_page_t,
    CONF_ROLLER: lv_roller_t,
    CONF_SLIDER: lv_slider_t,
    CONF_SPINNER: lv_spinner_t,
    CONF_SWITCH: lv_switch_t,
    CONF_SPINBOX: lv_spinbox_t,
    CONF_TABVIEW: lv_tabview_t,
    CONF_TEXTAREA: lv_textarea_t,
    CONF_TILEVIEW: lv_tileview_t,
}


def get_widget_type(typestr: str) -> LvType:
    return LV_TYPES[typestr]


def generate_id(base):
    generate_id.counter += 1
    return f"lvgl_{base}_{generate_id.counter}"


generate_id.counter = 0


class LValidator:
    def __init__(self, validator, rtype, idtype=None, idexpr=None, retmapper=None):
        self.validator = validator
        self.rtype = rtype
        self.idtype = idtype
        self.idexpr = idexpr
        self.retmapper = retmapper

    def __call__(self, value):
        if isinstance(value, cv.Lambda):
            return cv.returning_lambda(value)
        if self.idtype is not None and isinstance(value, ID):
            return cv.use_id(self.idtype)(value)
        return self.validator(value)

    async def process(self, value, args=()):
        if value is None:
            return None
        if isinstance(value, Lambda):
            return f"{await cg.process_lambda(value, args, return_type=self.rtype)}()"
        if self.idtype is not None and isinstance(value, ID):
            return f"{value}->{self.idexpr};"
        if self.retmapper is not None:
            return self.retmapper(value)
        return value
