from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_LOOP
from esphome.components.file.image import image_schema, write_image
from esphome.components.image import Image_, validate_settings
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_REPEAT
from esphome.types import ConfigType

CODEOWNERS = ["@syndlex"]
AUTO_LOAD = ["file"]
DEPENDENCIES = ["display"]

CONF_START_FRAME = "start_frame"
CONF_END_FRAME = "end_frame"
CONF_FRAME = "frame"

animation_ns = cg.esphome_ns.namespace("animation")

Animation_ = animation_ns.class_("Animation", Image_)

# Actions
NextFrameAction = animation_ns.class_(
    "AnimationNextFrameAction", automation.Action, cg.Parented.template(Animation_)
)
PrevFrameAction = animation_ns.class_(
    "AnimationPrevFrameAction", automation.Action, cg.Parented.template(Animation_)
)
SetFrameAction = animation_ns.class_(
    "AnimationSetFrameAction", automation.Action, cg.Parented.template(Animation_)
)

ANIMATION_SCHEMA = image_schema(Animation_).extend(
    {
        cv.Optional(CONF_LOOP): cv.All(
            {
                cv.Optional(CONF_START_FRAME, default=0): cv.positive_int,
                cv.Optional(CONF_END_FRAME): cv.positive_int,
                cv.Optional(CONF_REPEAT): cv.positive_int,
            }
        ),
    },
)

# Shared schema used by both the (deprecated) top-level `animation:` key and the
# `image:` `platform: animation` entry.
ANIMATION_CONFIG_SCHEMA = cv.All(ANIMATION_SCHEMA, validate_settings)


NEXT_FRAME_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Animation_),
    }
)
PREV_FRAME_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Animation_),
    }
)
SET_FRAME_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Animation_),
        cv.Required(CONF_FRAME): cv.uint16_t,
    }
)


@automation.register_action(
    "animation.next_frame", NextFrameAction, NEXT_FRAME_SCHEMA, synchronous=True
)
@automation.register_action(
    "animation.prev_frame", PrevFrameAction, PREV_FRAME_SCHEMA, synchronous=True
)
@automation.register_action(
    "animation.set_frame", SetFrameAction, SET_FRAME_SCHEMA, synchronous=True
)
async def animation_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if (frame := config.get(CONF_FRAME)) is not None:
        template_ = await cg.templatable(frame, args, cg.uint16)
        cg.add(var.set_frame(template_))
    return var


async def setup_animation(config: ConfigType) -> None:
    (
        prog_arr,
        width,
        height,
        image_type,
        trans_value,
        frame_count,
    ) = await write_image(config, all_frames=True)

    var = cg.new_Pvariable(
        config[CONF_ID],
        prog_arr,
        width,
        height,
        frame_count,
        image_type,
        trans_value,
    )
    if loop_config := config.get(CONF_LOOP):
        start = loop_config[CONF_START_FRAME]
        end = loop_config.get(CONF_END_FRAME, frame_count)
        count = loop_config.get(CONF_REPEAT, -1)
        cg.add(var.set_loop(start, end, count))


CONFIG_SCHEMA = ANIMATION_CONFIG_SCHEMA

to_code = setup_animation
