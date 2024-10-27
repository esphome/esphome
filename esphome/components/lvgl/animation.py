from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.lvgl.lvcode import LambdaContext, LvglComponent
from esphome.components.lvgl.schemas import STYLE_PROPS
from esphome.components.lvgl.types import LvAnimation, LvglAction, lv_color_t, lv_obj_t
from esphome.config_validation import COMPONENT_SCHEMA
from esphome.const import (
    CONF_ACCELERATION,
    CONF_DURATION,
    CONF_FROM,
    CONF_ID,
    CONF_TIMING,
    CONF_TO,
    CONF_TYPE,
    CONF_WEIGHT,
)
from esphome.cpp_generator import TemplateArguments

from .defines import (
    CONF_ANIMATIONS,
    CONF_LVGL_ID,
    CONF_WIDGETS,
    LValidator,
    add_define,
    literal,
)
from .lv_validation import color, get_component_colors, lv_color
from .lvcode import LVGL_COMP_ARG
from .types import lvgl_ns
from .widgets import get_widgets

LvAnimationTimingRoundTrip = lvgl_ns.class_("LvAnimationTimingRoundTrip")
LvAnimationTimingEaseInOut = lvgl_ns.class_("LvAnimationTimingEaseInOut")

CONF_BOUNCE = "bounce"


def timing_class(name, extras=None):
    # Convert config option to camel case
    cls_name = "LvAnimationTiming" + "".join([w.capitalize() for w in name.split("_")])
    cls = lvgl_ns.class_(cls_name)
    schema = cv.Schema({cv.GenerateID(): cv.declare_id(cls)})
    if extras:
        schema = schema.extend(extras)
    return name, schema


TIMING_SCHEMA = cv.maybe_simple_value(
    cv.typed_schema(
        dict(
            [
                timing_class("round_trip"),
                timing_class(
                    "ease_in_out",
                    {cv.Optional(CONF_WEIGHT, default=0.5): cv.zero_to_one_float},
                ),
                timing_class(
                    "gravity",
                    {
                        cv.Optional(CONF_BOUNCE, default=0.5): cv.zero_to_one_float,
                        cv.Optional(
                            CONF_ACCELERATION, default=0.5
                        ): cv.zero_to_one_float,
                    },
                ),
            ]
        ),
        default_type="ease_in_out",
    ),
    key=CONF_TYPE,
)

CONF_START_DELAY = "start_delay"

literal_color = LValidator(
    color, lv_color_t, retmapper=get_component_colors, animatable=True
)


def from_to(validator):
    return cv.Schema(
        {
            cv.Required(CONF_FROM): validator,
            cv.Required(CONF_TO): validator,
        }
    )


# Colors can only be animated between constants, not lambdas.
def map_v(validator):
    if validator == lv_color:
        return literal_color
    return validator


ANIMABLE_STYLES = {
    k: map_v(v)
    for k, v in STYLE_PROPS.items()
    if isinstance(v, LValidator) and v.animatable
}

ANIMATION_CONFIG = cv.Schema(
    {
        cv.Optional(CONF_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_START_DELAY): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TIMING, default={}): cv.ensure_list(TIMING_SCHEMA),
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
            ).extend({cv.Optional(k): from_to(v) for k, v in ANIMABLE_STYLES.items()})
        ),
    }
).extend(COMPONENT_SCHEMA)


async def process_arg(validator, arg) -> list:
    if validator == literal_color:
        value = get_component_colors(arg)
    else:
        value = [await validator.process(arg, raw_lambda=True)]
    return [literal(f"TemplatableValue<uint32_t>({v})") for v in value]


def process_value(validator, index):
    if validator != literal_color:
        return literal(f"values[{index}]")
    return literal(
        f"lv_color_make(values[{index}+0], values[{index}+1], values[{index}+2])"
    )


async def animations_to_code(config):
    for animation in config.get(CONF_ANIMATIONS, []):
        add_define("USE_LVGL_ANIMATION")
        widgets = animation[CONF_WIDGETS]
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
                for prop, limits in props:
                    validator = ANIMABLE_STYLES[prop]
                    value = process_value(validator, len(froms))
                    w.set_style(prop, value, 0)
                    froms.extend(await process_arg(validator, limits[CONF_FROM]))
                    tos.extend(await process_arg(validator, limits[CONF_TO]))

        data_size = len(froms)
        var = cg.new_Pvariable(
            animation[CONF_ID],
            TemplateArguments(data_size),
            await ctx.get_lambda(),
            froms,
            tos,
        )
        for timing in animation[CONF_TIMING]:
            timing_id = timing[CONF_ID]
            args = sorted(
                [v for k, v in timing.items() if k not in [CONF_ID, CONF_TYPE]]
            )
            timing_var = cg.new_Pvariable(timing_id, *args)
            cg.add(var.add_timing(timing_var))
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
