import esphome.codegen as cg
from esphome import automation
from esphome.components.key_provider import KeyProvider
from .defines import CONF_TEXT
from esphome.const import CONF_VALUE

uint16_t_ptr = cg.uint16.operator("ptr")
lvgl_ns = cg.esphome_ns.namespace("lvgl")
char_ptr = cg.global_ns.namespace("char").operator("ptr")
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


class LvType(cg.MockObjClass):

    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents", ())
        super().__init__(*args, parents=parens + (lv_obj_base_t,))
        self.args = kwargs.pop("largs", [(lv_obj_t_ptr, "obj")])
        self.value = kwargs.pop("lvalue", lambda w: w.obj)
        self.has_on_value = kwargs.pop("has_on_value", False)
        self.animated = kwargs.pop("animated", False)
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
LvBtnmBtn = LvBoolean(str(cg.uint16), parents=(lv_pseudo_button_t,))
lv_dropdown_list_t = LvType("lv_dropdown_list_t")
lv_meter_t = LvType("lv_meter_t")
lv_btn_t = LvBoolean("lv_btn_t")
lv_checkbox_t = LvBoolean("lv_checkbox_t")
lv_line_t = LvType("lv_line_t")
lv_img_t = LvType("lv_img_t")
lv_animimg_t = LvType("lv_animimg_t")
lv_tile_t = LvType("lv_tileview_tile_t")
lv_spinbox_t = LvNumber("lv_spinbox_t")
lv_arc_t = LvNumber("lv_arc_t")
lv_bar_t = LvNumber("lv_bar_t")
lv_slider_t = LvNumber("lv_slider_t")
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
lv_canvas_t = LvType("lv_canvas_t")
lv_dropdown_t = LvSelect("lv_dropdown_t")
lv_roller_t = LvSelect("lv_roller_t", animated=True)
lv_switch_t = LvBoolean("lv_switch_t")
lv_table_t = LvType("lv_table_t")
lv_chart_t = LvType("lv_chart_t")
lv_btnmatrix_t = LvType(
    "LvBtnmatrixType",
    parents=(KeyProvider, LvCompound),
    largs=[(uint16_t_ptr, "x")],
    lvalue=lambda w: f"{w.var}->get_selected( )",
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
lv_spinner_t = lv_obj_t
lv_ticks_t = lv_obj_t
lv_tick_style_t = lv_obj_t


def get_widget_type(typestr: str) -> LvType:
    return globals()[f"lv_{typestr}_t"]
