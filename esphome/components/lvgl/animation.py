from esphome import automation, codegen as cg, config_validation as cv
from esphome.automation import Trigger, build_automation
from esphome.config_validation import COMPONENT_SCHEMA
from esphome.const import (
    CONF_ACCELERATION,
    CONF_DURATION,
    CONF_FROM,
    CONF_ID,
    CONF_ON_START,
    CONF_TIMING,
    CONF_TO,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_WEIGHT,
)
from esphome.cpp_generator import MockObj, TemplateArguments

from ..const import CONF_LOOP
from .defines import (
    CONF_AUTO_START,
    CONF_LVGL_ID,
    CONF_ON_STOP,
    CONF_WIDGETS,
    LValidator,
    add_define,
    literal,
)
from .lv_validation import (
    color,
    get_component_colors,
    lv_color,
    lv_milliseconds,
    lv_positive_float,
    lv_zero_to_one_float,
)
from .lvcode import LVGL_COMP_ARG, LambdaContext, LvglComponent, lv_add
from .schemas import STYLE_PROPS
from .types import LvAnimation, LvglAction, lv_color_t, lv_coord_t, lv_obj_t, lvgl_ns
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


# TODO - currently the order of arguments to timing classes is expected to be alphabetical, but this is not enforced.
# It would be better to have a more robust way of passing arguments to the timing classes.
TIMING_SCHEMA = cv.maybe_simple_value(
    cv.typed_schema(
        dict(
            [
                timing_class("round_trip"),
                timing_class(
                    "ease_in_out",
                    {cv.Optional(CONF_WEIGHT, default=2.0): lv_positive_float},
                ),
                timing_class(
                    "gravity",
                    {
                        cv.Optional(CONF_ACCELERATION, default=0.5): lv_positive_float,
                        cv.Optional(CONF_BOUNCE, default=0.5): lv_zero_to_one_float,
                    },
                ),
            ]
        ),
        default_type="ease_in_out",
    ),
    key=CONF_TYPE,
)

CONF_START_DELAY = "start_delay"


class LiteralColorValidator(LValidator):
    def __init__(self):
        super().__init__(
            color, lv_color_t, retmapper=get_component_colors, animatable=True
        )

    def __call__(self, value):
        if isinstance(value, cv.Lambda):
            raise cv.Invalid(
                "An animated color may not be set with a lambda, only a literal color value."
            )
        return super().__call__(value)


literal_color = LiteralColorValidator()


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

ANIMATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_AUTO_START, default=False): cv.boolean,
        cv.Optional(CONF_LOOP, default=False): cv.boolean,
        cv.Optional(CONF_DURATION, default="5s"): lv_milliseconds,
        cv.Optional(CONF_START_DELAY, default="0s"): lv_milliseconds,
        cv.Optional(CONF_TIMING, default=[]): cv.ensure_list(TIMING_SCHEMA),
        cv.Required(CONF_ID): cv.declare_id(LvAnimation),
        cv.Optional(CONF_ON_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger.template()),
            }
        ),
        cv.Optional(CONF_ON_STOP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger.template()),
            }
        ),
        cv.Required(CONF_WIDGETS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_ID): cv.use_id(lv_obj_t),
                }
            ).extend({cv.Optional(k): from_to(v) for k, v in ANIMABLE_STYLES.items()})
        ),
    }
).extend(COMPONENT_SCHEMA)


async def _process_arg(validator, arg) -> list:
    # from/to values are evaluated at animation start with no arguments, so the
    # generated lambda must be parameterless rather than inheriting the enclosing
    # update-callback's `values` parameter.
    value = await validator.process(arg, args=[], raw_lambda=True)
    value = list(value) if isinstance(value, tuple) else [value]
    return [literal(f"TemplatableValue<lv_coord_t>({v})") for v in value]


async def animations_to_code(config):
    for animation in config:
        add_define("USE_LVGL_ANIMATION")
        widgets = animation[CONF_WIDGETS]
        async with LambdaContext(
            [(lv_coord_t.operator("const").operator("ptr"), "values")]
        ) as ctx:
            froms = []
            tos = []
            for widget in widgets:
                w = (await get_widgets(widget))[0]
                props = [(k, v) for k, v in widget.items() if k in ANIMABLE_STYLES]
                for prop, value_range in props:
                    # prop is the style property, value_range is a dict with from: and to: values
                    validator = ANIMABLE_STYLES[prop]
                    from_value = await _process_arg(validator, value_range[CONF_FROM])
                    to_value = await _process_arg(validator, value_range[CONF_TO])
                    index = len(froms)
                    if len(from_value) == 1:
                        value = f"values[{index}]"
                    else:
                        value = f"lv_color_make(values[{index}+0], values[{index}+1], values[{index}+2])"
                    w.set_style(prop, literal(value), 0)
                    # The value arrays are extended by 1 item for scalar properties, 3 for colors
                    froms.extend(from_value)
                    tos.extend(to_value)

        data_size = len(froms)
        loop = animation[CONF_LOOP]
        start_delay = await lv_milliseconds.process(animation.get(CONF_START_DELAY))
        var = cg.new_Pvariable(
            animation[CONF_ID],
            TemplateArguments(data_size, animation[CONF_AUTO_START]),
            await ctx.get_lambda(),
            froms,
            tos,
        )
        for timing in animation[CONF_TIMING]:
            timing_id = timing[CONF_ID]
            args = sorted(
                [(k, v) for k, v in timing.items() if k not in [CONF_ID, CONF_TYPE]]
            )
            args = [v for k, v in args]
            timing_var = cg.new_Pvariable(timing_id, *args)
            cg.add(var.add_timing(timing_var))

        if start_delay:
            cg.add(var.set_start_delay(start_delay))
        if loop:
            cg.add(var.set_loop(loop))
        cg.add(
            var.set_duration(await lv_milliseconds.process(animation[CONF_DURATION]))
        )
        await cg.register_component(var, animation)


async def add_animation_triggers(config):
    async def add_triggers(animation: MockObj, event: str, config: dict) -> None:
        for conf in config:
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await build_automation(trigger, [], conf)
            async with LambdaContext([]) as context:
                lv_add(trigger.trigger())
            lv_add(
                getattr(
                    animation,
                    f"add_{event}_callback",
                )(await context.get_lambda())
            )

    for animation in config:
        var = await cg.get_variable(animation[CONF_ID])
        await add_triggers(var, CONF_ON_START, animation.get(CONF_ON_START, []))
        await add_triggers(var, CONF_ON_STOP, animation.get(CONF_ON_STOP, []))


@automation.register_action(
    "lvgl.animation.start",
    LvglAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.ensure_list(cv.use_id(LvAnimation)),
            cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
            cv.Optional(CONF_DURATION): lv_milliseconds,
            cv.Optional(CONF_START_DELAY): lv_milliseconds,
            cv.Optional(CONF_LOOP): cv.boolean,
        },
        key=CONF_ID,
    ),
    synchronous=True,
)
async def start_animation(config, action_id, template_arg, args):
    animations = config[CONF_ID]
    loop = config.get(CONF_LOOP)
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        for animation in animations:
            anim_var = await cg.get_variable(animation)
            if loop is not None:
                context.add(anim_var.set_loop(loop))
            if (duration := config.get(CONF_DURATION)) is not None:
                context.add(
                    anim_var.set_duration(await lv_milliseconds.process(duration))
                )
            if (start_delay := config.get(CONF_START_DELAY)) is not None:
                context.add(
                    anim_var.set_start_delay(await lv_milliseconds.process(start_delay))
                )
            context.add(anim_var.start())
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var


@automation.register_action(
    "lvgl.animation.stop",
    LvglAction,
    cv.maybe_simple_value(
        {
            cv.Required(CONF_ID): cv.ensure_list(cv.use_id(LvAnimation)),
            cv.GenerateID(CONF_LVGL_ID): cv.use_id(LvglComponent),
        },
        key=CONF_ID,
    ),
    synchronous=True,
)
async def stop_animation(config, action_id, template_arg, args):
    animations = config[CONF_ID]
    async with LambdaContext(LVGL_COMP_ARG, where=action_id) as context:
        for animation in animations:
            anim_var = await cg.get_variable(animation)
            context.add(anim_var.stop())
    var = cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
    await cg.register_parented(var, config[CONF_LVGL_ID])
    return var
