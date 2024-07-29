import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import Lambda
from esphome.cpp_types import nullptr

from ...cpp_generator import RawStatement
from .defines import literal
from .lvcode import LambdaContext, lv, lv_add
from .widget import Widget, get_widget, set_obj_properties


async def action_to_code(action: list, action_id, widget: Widget, template_arg, args):
    with LambdaContext() as context:
        lv.cond_if(widget.obj == nullptr)
        lv_add(RawStatement("  return;"))
        lv.cond_endif()
    code = context.get_code()
    code.extend(action)
    action = "\n".join(code) + "\n\n"
    lamb = await cg.process_lambda(Lambda(action), args)
    var = cg.new_Pvariable(action_id, template_arg, lamb)
    return var


async def update_to_code(config, action_id, template_arg, args):
    if config is not None:
        widget = await get_widget(config[CONF_ID])
        with LambdaContext() as context:
            await set_obj_properties(widget, config)
            await widget.type.to_code(widget, config)
            if (
                widget.type.w_type.value_property is not None
                and widget.type.w_type.value_property in config
            ):
                lv.event_send(widget.obj, literal("LV_EVENT_VALUE_CHANGED"), nullptr)
        return await action_to_code(
            context.get_code(), action_id, widget, template_arg, args
        )
