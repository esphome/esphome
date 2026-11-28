"""Run a TFLite model on frames from an esp32_camera.

The camera delivers JPEG frames; the camera_decoder component decodes them to
RGB888 and the image pipeline from tflite_image scales and fills the model's
input tensor. The model, the options and the classification decoder are the
same as an image entry of tflite_image.
"""

import esphome.codegen as cg
from esphome.components import esp32, esp32_camera
from esphome.components.camera_decoder import ESP32CameraJPEGDecoder
from esphome.components.tflite_image import (
    CONF_FRAME_HEIGHT,
    CONF_FRAME_WIDTH,
    IMAGE_SCHEMA,
    configure as configure_image,
)
from esphome.components.tflite_micro_runtime import (
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
import esphome.config_validation as cv
from esphome.const import CONF_ROTATION
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_camera"

AUTO_LOAD = [
    "camera",
    "camera_decoder",
    "tflite_image",
    "tflite_micro_runtime",
    "tflite_output_processor",
]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_CAMERA_ID = "camera_id"
CONF_MIN_INTERVAL = "min_interval"
CONF_LISTENER_ID = "listener_id"
CONF_JPEG_DECODER_ID = "jpeg_decoder_id"

camera_ns = cg.esphome_ns.namespace("camera")
CameraListener = camera_ns.class_("CameraListener")
PixelFormat = camera_ns.enum("PixelFormat")
# The image pipeline reads RGB888; cv.enum turns the value into the enum
# expression the decoder constructor takes.
PIXEL_FORMAT_RGB888 = cv.enum({"RGB888": PixelFormat.PIXEL_FORMAT_RGB888})("RGB888")

tflite_camera_ns = cg.esphome_ns.namespace("tflite_camera")
TFLiteCamera = tflite_camera_ns.class_("TFLiteCamera", cg.Component, CameraListener)

# The decoded frame carries its own size, so the pipeline takes the frame
# geometry from the decoded spec at run time. A configured frame size would be
# a second, possibly wrong, source of truth for the same value.
CAMERA_IMAGE_SCHEMA = cv.Schema(
    {
        key: value
        for key, value in IMAGE_SCHEMA.schema.items()
        if key not in (CONF_FRAME_WIDTH, CONF_FRAME_HEIGHT)
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(CAMERA_IMAGE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
            cv.Optional(CONF_ROTATION, default=0): cv.one_of(0, 90, 180, 270),
            cv.Optional(
                CONF_MIN_INTERVAL, default="1ms"
            ): cv.positive_time_period_milliseconds,
            cv.GenerateID(CONF_LISTENER_ID): cv.declare_id(TFLiteCamera),
            cv.GenerateID(CONF_JPEG_DECODER_ID): cv.declare_id(ESP32CameraJPEGDecoder),
        }
    ),
    cv.only_on_esp32,
)


def _final_validate(config: ConfigType) -> None:
    """Validate the model and that the selected camera delivers JPEG frames."""
    final_validate(config)
    camera_config = fv.get_declaring_config(config[CONF_CAMERA_ID])
    if (
        camera_config[esp32_camera.CONF_PIXEL_FORMAT] != "JPEG"
        and camera_config.get(esp32_camera.CONF_JPEG_QUALITY, 0) == 0
    ):
        raise cv.Invalid(
            "tflite_camera requires JPEG frames; set pixel_format: JPEG or enable "
            "JPEG conversion with a nonzero jpeg_quality on the selected esp32_camera"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    """Create the runtime, the image pipeline and the camera listener."""
    var, model_path = await declare_model(config)
    await configure_image(var, config, model_path)

    # camera_decoder is auto-loaded with the decoder object created below, so its
    # own to_code() does not run; emit the define and the library it needs here.
    cg.add_define("USE_CAMERA_DECODER")
    esp32.add_idf_component(name="espressif/esp_new_jpeg", ref="1.0.2")
    decoder = cg.new_Pvariable(
        config[CONF_JPEG_DECODER_ID],
        PIXEL_FORMAT_RGB888.enum_value,
        config[CONF_ROTATION],
        0,  # no preallocation: the buffer is sized by the first decoded frame
    )
    listener = cg.new_Pvariable(config[CONF_LISTENER_ID], var)
    cg.add(listener.set_decoder(decoder))
    camera_var = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(listener.set_camera(camera_var))
    cg.add(listener.set_min_interval(config[CONF_MIN_INTERVAL]))
    await cg.register_component(listener, config)
