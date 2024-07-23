from esphome import codegen as cg
from esphome.core import ID

from .defines import CONF_LABEL, CONF_OBJ, CONF_TEXT

uint16_t_ptr = cg.uint16.operator("ptr")
lvgl_ns = cg.esphome_ns.namespace("lvgl")
char_ptr = cg.global_ns.namespace("char").operator("ptr")
void_ptr = cg.void.operator("ptr")
LvglComponent = lvgl_ns.class_("LvglComponent", cg.PollingComponent)
lv_event_code_t = cg.global_ns.namespace("lv_event_code_t")
FontEngine = lvgl_ns.class_("FontEngine")
LvCompound = lvgl_ns.class_("LvCompound")
lv_font_t = cg.global_ns.class_("lv_font_t")
lv_style_t = cg.global_ns.struct("lv_style_t")
lv_pseudo_button_t = lvgl_ns.class_("LvPseudoButton")
lv_obj_base_t = cg.global_ns.class_("lv_obj_t", lv_pseudo_button_t)
lv_obj_t_ptr = lv_obj_base_t.operator("ptr")
lv_disp_t_ptr = cg.global_ns.struct("lv_disp_t").operator("ptr")
lv_color_t = cg.global_ns.struct("lv_color_t")


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


class LvText(LvType):
    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            largs=[(cg.std_string, "text")],
            lvalue=lambda w: w.get_property("text")[0],
            **kwargs,
        )
        self.value_property = CONF_TEXT


lv_obj_t = LvType("lv_obj_t")
lv_label_t = LvText("lv_label_t")

LV_TYPES = {
    CONF_LABEL: lv_label_t,
    CONF_OBJ: lv_obj_t,
}


def get_widget_type(typestr: str) -> LvType:
    return LV_TYPES[typestr]


CUSTOM_EVENT = ID("lv_custom_event", False, type=lv_event_code_t)
