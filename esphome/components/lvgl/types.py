from esphome import automation, codegen as cg
from esphome.const import CONF_TEXT, CONF_VALUE
from esphome.cpp_generator import MockObj
from esphome.cpp_types import esphome_ns

from .defines import lvgl_ns


class LvType(cg.MockObjClass):
    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents", ())
        super().__init__(*args, parents=parens + (lv_obj_base_t,))
        self.args = kwargs.pop("largs", [(lv_obj_t_ptr, "obj")])
        self.value = kwargs.pop("lvalue", lambda w: w.obj)
        self.has_on_value = kwargs.pop("has_on_value", False)
        self.value_property = None

    def get_arg_type(self):
        if len(self.args) == 0:
            return None
        return [arg[0] for arg in self.args]

    @property
    def name(self):
        return self.base.removeprefix("lv_").removesuffix("_t")


class LvNumber(LvType):
    def __init__(self, *args):
        super().__init__(
            *args,
            largs=[(cg.float_, "x")],
            lvalue=lambda w: w.get_number_value(),
            has_on_value=True,
        )
        self.value_property = CONF_VALUE


uint16_t_ptr = cg.uint16.operator("ptr")
char_ptr = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
lv_coord_t = cg.global_ns.namespace("lv_coord_t")
lv_event_code_t = cg.global_ns.enum("lv_event_code_t")
lv_indev_type_t = cg.global_ns.enum("lv_indev_type_t")
lv_key_t = cg.global_ns.enum("lv_key_t")
PlainTrigger = esphome_ns.class_("Trigger<>", automation.Trigger.template())
DrawEndTrigger = esphome_ns.class_(
    "Trigger<uint32_t, uint32_t>", automation.Trigger.template(cg.uint32, cg.uint32)
)
IdleTrigger = lvgl_ns.class_("IdleTrigger", automation.Trigger.template())
ObjUpdateAction = lvgl_ns.class_("ObjUpdateAction", automation.Action)
LvglCondition = lvgl_ns.class_("LvglCondition", automation.Condition)
LvglAction = lvgl_ns.class_("LvglAction", automation.Action)
lv_lambda_t = lvgl_ns.class_("LvLambdaType")
LvCompound = lvgl_ns.class_("LvCompound")
lv_font_t = cg.global_ns.class_("lv_font_t")
lv_style_t = cg.global_ns.struct("lv_style_t")
# fake parent class for first class widgets and matrix buttons
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
lv_obj_base_t = cg.global_ns.class_("lv_obj_t", lv_pseudo_button_t)
lv_obj_t_ptr = lv_obj_base_t.operator("ptr")
lv_disp_t = cg.global_ns.struct("lv_disp_t")
lv_color_t = cg.global_ns.struct("lv_color_t")
lv_opa_t = cg.global_ns.struct("lv_opa_t")
lv_group_t = cg.global_ns.struct("lv_group_t")
LVTouchListener = lvgl_ns.class_("LVTouchListener")
LVEncoderListener = lvgl_ns.class_("LVEncoderListener")
lv_obj_t = LvType("lv_obj_t")
lv_page_t = LvType("LvPageType", parents=(LvCompound,))
lv_img_t = LvType("lv_img_t")
lv_gradient_t = LvType("lv_grad_dsc_t")
lv_event_t = LvType("lv_event_t")

LV_EVENT = MockObj(base="LV_EVENT_", op="")
LV_STATE = MockObj(base="LV_STATE_", op="")
LV_BTNMATRIX_CTRL = MockObj(base="LV_BUTTONMATRIX_CTRL_", op="")


class LvText(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.std_string, "text")],
            lvalue=lambda w: w.get_property("text"),
            has_on_value=True,
            **kwargs,
        )
        self.value_property = CONF_TEXT


class LvBoolean(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.bool_, "x")],
            lvalue=lambda w: w.has_state(LV_STATE.CHECKED),
            has_on_value=True,
            **kwargs,
        )


class LvSelect(LvType):
    def __init__(self, *args, **kwargs):
        parens = kwargs.pop("parents", ()) + (LvCompound,)
        super().__init__(
            *args,
            largs=[(cg.int_, "x"), (cg.std_string, "text")],
            lvalue=lambda w: [w.var.get_selected_index(), w.var.get_selected_text()],
            has_on_value=True,
            parents=parens,
            **kwargs,
        )
