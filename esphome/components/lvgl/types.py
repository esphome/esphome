import esphome.codegen as cg
from esphome import automation
from esphome.components.key_provider import KeyProvider

lvgl_ns = cg.esphome_ns.namespace("lvgl")
char_ptr_const = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
lv_coord_t = cg.global_ns.namespace("lv_coord_t")
lv_event_code_t = cg.global_ns.enum("lv_event_code_t")
LvglComponent = lvgl_ns.class_("LvglComponent", cg.PollingComponent)
LvglComponentPtr = LvglComponent.operator("ptr")
# LvImagePtr = Image_.operator("ptr")
LVTouchListener = lvgl_ns.class_("LVTouchListener")
LVRotaryEncoderListener = lvgl_ns.class_("LVRotaryEncoderListener")
IdleTrigger = lvgl_ns.class_("IdleTrigger", automation.Trigger.template())
# FontEngine = lvgl_ns.class_("FontEngine")
ObjUpdateAction = lvgl_ns.class_("ObjUpdateAction", automation.Action)
LvglCondition = lvgl_ns.class_("LvglCondition", automation.Condition)
LvglAction = lvgl_ns.class_("LvglAction", automation.Action)
lv_lambda_t = lvgl_ns.class_("LvLambdaType")
lv_lambda_ptr_t = lvgl_ns.class_("LvLambdaType").operator("ptr")
LvCompound = lvgl_ns.class_("LvCompound")
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
lv_font_t = cg.global_ns.class_("LvFontType")
lv_page_t = cg.global_ns.class_("LvPageType")
lv_screen_t = cg.global_ns.class_("LvScreenType")
lv_point_t = cg.global_ns.struct("LvPointType")
lv_msgbox_t = cg.global_ns.struct("LvMsgBoxType")
lv_style_t = cg.global_ns.struct("LvStyleType")
lv_color_t = cg.global_ns.struct("LvColorType")
lv_meter_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
lv_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
lv_meter_indicator_t_ptr = lv_meter_indicator_t.operator("ptr")
lv_obj_base_t = cg.global_ns.class_("LvObjType", lv_pseudo_button_t)
lv_obj_t_ptr = lv_obj_base_t.operator("ptr")


class LvType(cg.MockObjClass):

    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents", ())
        super().__init__(*args, parents=parens + (lv_obj_base_t,))
        self.args = kwargs.pop("largs", [(lv_obj_t_ptr, "obj")])
        self.value = kwargs.pop("lvalue", lambda w: w.obj)
        self.has_on_value = kwargs.pop("has_on_value", False)

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


lv_obj_t = LvType("LvObjType")
LvBtnmBtn = LvType("LvBtnmBtn", parents=(lv_pseudo_button_t,))
lv_label_t = LvType("LvLabelType")
lv_dropdown_list_t = LvType("LvDropdownListType")
lv_meter_t = LvType("LvMeterType")
lv_btn_t = LvType("LvBtnType")
lv_checkbox_t = LvType("LvCheckboxType")
lv_line_t = LvType("LvLineType")
lv_img_t = LvType("LvImgType")
lv_animimg_t = LvType("LvAnimImgType")
lv_tile_t = LvType("LvTileType")
lv_spinbox_t = LvNumber("LvSpinBoxType")
lv_arc_t = LvNumber("LvArcType")
lv_bar_t = LvNumber("LvBarType")
lv_slider_t = LvNumber("LvSliderType")
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
lv_canvas_t = LvType("LvCanvasType")
lv_select_t = lvgl_ns.class_("LvPseudoSelect")
lv_dropdown_t = LvType("LvDropdownType", parents=(lv_select_t,))
lv_roller_t = LvType("LvRollerType", parents=(lv_select_t,))
lv_led_t = LvType("LvLedType")
lv_switch_t = LvType("LvSwitchType")
lv_table_t = LvType("LvTableType")
lv_chart_t = LvType("LvChartType")
lv_btnmatrix_t = LvType("LvBtnmatrixType", parents=(KeyProvider, LvCompound))
lv_keyboard_t = LvType(
    "LvKeyboardType",
    parents=(KeyProvider, LvCompound),
    largs=[(cg.const_char_ptr, "text")],
    lvalue=lambda w: f"lv_textarea_get_text({w.obj})",
)
lv_textarea_t = LvType(
    "LvTextareaType",
    largs=[(cg.const_char_ptr, "text")],
    lvalue=lambda w: f"lv_textarea_get_text({w.obj})",
    has_on_value=True,
)

lv_tileview_t = LvType(
    "LvTileViewtype",
    largs=[(lv_obj_t_ptr, "tile")],
    lvalue=lambda w: f"lv_tileview_get_tile_act({w.obj})",
)
lv_spinner_t = lv_obj_t
lv_ticks_t = lv_obj_t
lv_tick_style_t = lv_obj_t


def get_widget_type(typestr: str) -> cg.MockObjClass:
    return globals()[f"lv_{typestr}_t"]
