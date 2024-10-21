from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.lvgl.defines import (
    CONF_ANIMATIONS,
    CONF_WIDGETS,
    LValidator,
    add_define,
    literal,
)
from esphome.components.lvgl.lvcode import LambdaContext, LvglComponent
from esphome.components.lvgl.schemas import STYLE_PROPS
from esphome.components.lvgl.types import LvAnimation, LvglAction, lv_obj_t
from esphome.config_validation import COMPONENT_SCHEMA
from esphome.const import CONF_DURATION, CONF_FROM, CONF_ID, CONF_TO
from esphome.cpp_generator import TemplateArguments

from .defines import CONF_LVGL_ID
from .lvcode import LVGL_COMP_ARG
from .widgets import get_widgets

CONF_START_DELAY = "start_delay"


def from_to(validator):
    return cv.Schema(
        {
            cv.Required(CONF_FROM): validator,
            cv.Required(CONF_TO): validator,
        }
    )


ANIMABLE_STYLES = {
    k: from_to(v)
    for k, v in STYLE_PROPS.items()
    if isinstance(v, LValidator) and v.animatable
}

ANIMATION_CONFIG = cv.Schema(
    {
        cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_START_DELAY): cv.positive_time_period_milliseconds,
    }
)
ANIMATION_SCHEMA = ANIMATION_CONFIG.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(LvAnimation),
        cv.Required(CONF_WIDGETS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_ID): cv.use_id(lv_obj_t),
                }
            ).extend({cv.Optional(k): v for k, v in ANIMABLE_STYLES.items()})
        ),
    }
).extend(COMPONENT_SCHEMA)


async def process_arg(prop, arg):
    value = await STYLE_PROPS[prop].process(arg)
    return literal(f"TemplatableValue<uint32_t>({value})")


async def animations_to_code(config):
    for animation in config.get(CONF_ANIMATIONS, []):
        add_define("USE_LVGL_ANIMATION")
        widgets = animation[CONF_WIDGETS]
        # property_count = sum([len([k for k in widget if k in ANIMABLE_STYLES.keys()]) for widget in widgets])
        async with LambdaContext(
            [(cg.uint32.operator("const").operator("ptr"), "values")]
        ) as ctx:
            froms = []
            tos = []
            for widget in widgets:
                w = (await get_widgets(widget))[0]
                props = [
                    (k, v) for k, v in widget.items() if k in ANIMABLE_STYLES.keys()
                ]
                for prop in props:
                    validator = STYLE_PROPS[prop[0]]
                    value = literal(validator.from_int(f"values[{len(froms)}]"))
                    w.set_style(prop[0], value, 0)
                    froms.append(await process_arg(prop[0], prop[1][CONF_FROM]))
                    tos.append(await process_arg(prop[0], prop[1][CONF_TO]))

        data_size = len(froms)
        var = cg.new_Pvariable(
            animation[CONF_ID],
            TemplateArguments(data_size),
            await ctx.get_lambda(),
            froms,
            tos,
        )
        if CONF_DURATION in animation:
            cg.add(var.set_duration(animation[CONF_DURATION]))
        if CONF_START_DELAY in animation:
            cg.add(var.set_delay(animation[CONF_START_DELAY]))
        await cg.register_component(var, animation)


@automation.register_action(
    "lvgl.animation.start",
    LvglAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.ensure_list(cv.use_id(LvAnimation)),
            cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
            cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_START_DELAY): cv.positive_time_period_milliseconds,
        },
        key=CONF_ID,
    ),
)
async def start_animation(config, action_id, template_arg, args):
    animations = config[CONF_ID]
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        for animation in animations:
            anim_var = await cg.get_variable(animation)
            if (duration := config.get(CONF_DURATION)) is not None:
                context.add(anim_var.set_duration(duration))
            if (start_delay := config.get(CONF_START_DELAY)) is not None:
                context.add(anim_var.set_delay(start_delay))
            context.add(anim_var.start())
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var
