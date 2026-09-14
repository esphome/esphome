"""Live camera frames in an LVGL canvas (ESP32-P4).

Points an LVGL canvas straight at the frames an ``esp_video_camera`` captures.
The sensor already produces RGB565, which is what LVGL draws from, so nothing
is decoded and nothing is copied on the way.

A canvas rather than an image widget: the canvas is LVGL's supported way of
showing a buffer whose contents keep changing, and it is the one that has been
run against these sensors.
"""

from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.const import CONF_BYTE_ORDER, CONF_ENABLED
from esphome.components.esp_video_camera import ESPVideoCamera
from esphome.components.lvgl.widgets.canvas import lv_canvas_t
from esphome.const import CONF_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@youkorr"]
DEPENDENCIES = ["esp_video_camera", "lvgl"]

CONF_CAMERA_ID = "camera_id"
CONF_CANVAS_ID = "canvas_id"

lvgl_camera_display_ns = cg.esphome_ns.namespace("lvgl_camera_display")
LVGLCameraDisplay = lvgl_camera_display_ns.class_("LVGLCameraDisplay", cg.Component)
StartAction = lvgl_camera_display_ns.class_("StartAction", automation.Action)
StopAction = lvgl_camera_display_ns.class_("StopAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LVGLCameraDisplay),
        cv.Required(CONF_CAMERA_ID): cv.use_id(ESPVideoCamera),
        cv.Required(CONF_CANVAS_ID): cv.use_id(lv_canvas_t),
        cv.Optional(CONF_ENABLED, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(LVGLCameraDisplay)}
)


def _reject_swapped_byte_order(config: ConfigType) -> ConfigType:
    """The sensor's pixel order is fixed, so LVGL's has to be the matching one.

    The ISP writes RGB565 the way the chip stores a 16-bit word, and this
    component hands those bytes to LVGL untouched. Built for big_endian, LVGL
    reads every pixel with its two bytes the other way round, and the picture
    comes out in the wrong colours -- with nothing in the logs to say why.

    LVGL takes this from the display when the display states one, so this only
    fires where it really is a choice.
    """
    lvgl_configs = fv.full_config.get()["lvgl"]
    if any(c.get(CONF_BYTE_ORDER) == "big_endian" for c in lvgl_configs):
        raise cv.Invalid(
            "lvgl is set to big_endian, and camera frames cannot be shown that way: "
            "the sensor writes RGB565 in the chip's own byte order and this component "
            "does not copy the frames, so there is nowhere to swap them. Set "
            "'lvgl: byte_order: little_endian'. Note that LVGL defaults to big_endian "
            "unless the display states an order of its own."
        )
    return config


FINAL_VALIDATE_SCHEMA = _reject_swapped_byte_order


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(var.set_camera(camera))

    # The address of LVGL's widget variable, not its value: LVGL assigns those
    # variables while it builds the display, and this component is told about
    # the canvas before it necessarily exists.
    canvas = await cg.get_variable(config[CONF_CANVAS_ID])
    cg.add(var.set_canvas(cg.RawExpression(f"&{canvas}")))

    cg.add(var.set_enabled(config[CONF_ENABLED]))


@automation.register_action(
    "lvgl_camera_display.start", StartAction, ACTION_SCHEMA, synchronous=True
)
@automation.register_action(
    "lvgl_camera_display.stop", StopAction, ACTION_SCHEMA, synchronous=True
)
async def display_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
