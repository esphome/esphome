import esphome.codegen as cg
from esphome import automation
from esphome.components.key_provider import KeyProvider

lvgl_ns = cg.esphome_ns.namespace("lvgl")

char_ptr_const = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
lv_coord_t = cg.global_ns.namespace("lv_coord_t")
# lv_event_code_t = cg.global_ns.enum("lv_event_code_t")
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
# lv_lambda_ptr_t = lvgl_ns.class_("LvLambdaType").operator("ptr")

# Can't use the native type names here, since ESPHome munges variable names and they conflict
LvCompound = lvgl_ns.class_("LvCompound")
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
LvBtnmBtn = lvgl_ns.class_("LvBtnmBtn", lv_pseudo_button_t)
lv_obj_t = cg.global_ns.class_("LvObjType", lv_pseudo_button_t)
lv_font_t = cg.global_ns.class_("LvFontType")
lv_page_t = cg.global_ns.class_("LvPageType")
# lv_screen_t = cg.global_ns.class_("LvScreenType")
lv_point_t = cg.global_ns.struct("LvPointType")
# lv_msgbox_t = cg.global_ns.struct("LvMsgBoxType")
lv_obj_t_ptr = lv_obj_t.operator("ptr")
lv_style_t = cg.global_ns.struct("LvStyleType")
lv_color_t = cg.global_ns.struct("LvColorType")
# lv_theme_t = cg.global_ns.struct("LvThemeType")
# lv_theme_t_ptr = lv_theme_t.operator("ptr")
lv_meter_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
# lv_indicator_t = cg.global_ns.struct("LvMeterIndicatorType")
lv_meter_indicator_t_ptr = lv_meter_indicator_t.operator("ptr")
# lv_label_t = cg.MockObjClass("LvLabelType", parents=[lv_obj_t])
lv_dropdown_list_t = cg.MockObjClass("LvDropdownListType", parents=[lv_obj_t])
# lv_meter_t = cg.MockObjClass("LvMeterType", parents=[lv_obj_t])
# lv_btn_t = cg.MockObjClass("LvBtnType", parents=[lv_obj_t])
# lv_checkbox_t = cg.MockObjClass("LvCheckboxType", parents=[lv_obj_t])
# lv_line_t = cg.MockObjClass("LvLineType", parents=[lv_obj_t])
# lv_img_t = cg.MockObjClass("LvImgType", parents=[lv_obj_t])
lv_animimg_t = cg.MockObjClass("LvAnimImgType", parents=[lv_obj_t])
lv_number_t = lvgl_ns.class_("LvPseudoNumber")
lv_spinbox_t = cg.MockObjClass("LvSpinBoxType", parents=[lv_obj_t, lv_number_t])
lv_tileview_t = cg.MockObjClass("LvTileViewtype", parents=[lv_obj_t])
lv_tile_t = cg.MockObjClass("LvTileType", parents=[lv_obj_t])
# lv_arc_t = cg.MockObjClass("LvArcType", parents=[lv_obj_t, lv_number_t])
# lv_bar_t = cg.MockObjClass("LvBarType", parents=[lv_obj_t, lv_number_t])
# lv_slider_t = cg.MockObjClass("LvSliderType", parents=[lv_obj_t, lv_number_t])
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
# lv_canvas_t = cg.MockObjClass("LvCanvasType", parents=[lv_obj_t])
# lv_select_t = lvgl_ns.class_("LvPseudoSelect")
# lv_dropdown_t = cg.MockObjClass("LvDropdownType", parents=[lv_obj_t, lv_select_t])
# lv_roller_t = cg.MockObjClass("LvRollerType", parents=[lv_obj_t, lv_select_t])
# lv_led_t = cg.MockObjClass("LvLedType", parents=[lv_obj_t])
# lv_switch_t = cg.MockObjClass("LvSwitchType", parents=[lv_obj_t])
# lv_table_t = cg.MockObjClass("LvTableType", parents=[lv_obj_t])
# lv_textarea_t = cg.MockObjClass("LvTextareaType", parents=[lv_obj_t])
# lv_chart_t = cg.MockObjClass("LvChartType", parents=[lv_obj_t])
lv_btnmatrix_t = cg.MockObjClass(
    "LvBtnmatrixType", parents=[lv_obj_t, KeyProvider, LvCompound]
)
lv_keyboard_t = cg.MockObjClass(
    "LvKeyboardType", parents=[lv_obj_t, KeyProvider, LvCompound]
)
# Provided for the benefit of get_widget_type
# lv_spinner_t = lv_obj_t
# lv_ticks_t = lv_obj_t
# lv_tick_style_t = lv_obj_t
