import logging

from esphome import automation
import esphome.codegen as cg
import esphome.components.image as espImage
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_REPEAT

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["image"]
CODEOWNERS = ["@syndlex"]
DEPENDENCIES = ["display"]
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

CONF_LOOP = "loop"
CONF_START_FRAME = "start_frame"
CONF_END_FRAME = "end_frame"
CONF_FRAME = "frame"

animation_ns = cg.esphome_ns.namespace("animation")

Animation_ = animation_ns.class_("Animation", espImage.Image_)

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

CONFIG_SCHEMA = espImage.IMAGE_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(Animation_),
        cv.Optional(CONF_LOOP): cv.All(
            {
                cv.Optional(CONF_START_FRAME, default=0): cv.positive_int,
                cv.Optional(CONF_END_FRAME): cv.positive_int,
                cv.Optional(CONF_REPEAT): cv.positive_int,
            }
        ),
    },
)


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


@automation.register_action("animation.next_frame", NextFrameAction, NEXT_FRAME_SCHEMA)
@automation.register_action("animation.prev_frame", PrevFrameAction, PREV_FRAME_SCHEMA)
@automation.register_action("animation.set_frame", SetFrameAction, SET_FRAME_SCHEMA)
async def animation_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if (frame := config.get(CONF_FRAME)) is not None:
        template_ = await cg.templatable(frame, args, cg.uint16)
        cg.add(var.set_frame(template_))
    return var


async def to_code(config):
    (
        prog_arr,
        width,
        height,
        image_type,
        trans_value,
        frame_count,
    ) = await espImage.write_image(config, all_frames=True)

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
