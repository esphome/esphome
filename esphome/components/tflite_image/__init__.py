"""Image input pipeline for tflite_micro_runtime.

The model's input tensor supplies the geometry; the YAML values override it. The
pipeline fills the tensor from an RGB888 frame, applying the channel order,
normalization and inversion the model was trained with.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_model_io.model_reader import read_model_io
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_image"

# Library component: the runtime creates the pipeline from its own entry.
CONFIG_SCHEMA = cv.Schema({})

CONF_INPUT_TYPE = "input_type"
CONF_INPUT_CHANNELS = "input_channels"
CONF_INPUT_WIDTH = "input_width"
CONF_INPUT_HEIGHT = "input_height"
CONF_INPUT_ORDER = "input_order"
CONF_NORMALIZE = "normalize"
CONF_INVERT = "invert"
CONF_PIPELINE_ID = "pipeline_id"
CONF_DECODER_ID = "decoder_id"

tflite_image_ns = cg.esphome_ns.namespace("tflite_image")
ImageInputPipeline = tflite_image_ns.class_("ImageInputPipeline")
ChannelOrder = tflite_image_ns.enum("ChannelOrder", is_class=True)

output_ns = cg.esphome_ns.namespace("tflite_output_processor")
ClassificationDecoder = output_ns.class_("ClassificationDecoder")
OutputProcessing = output_ns.enum("OutputProcessing", is_class=True)

CHANNEL_ORDERS = {
    "RGB": ChannelOrder.CHANNEL_ORDER_RGB,
    "BGR": ChannelOrder.CHANNEL_ORDER_BGR,
    "GRAY": ChannelOrder.CHANNEL_ORDER_GRAY,
}
INPUT_TYPES = {"uint8", "float32"}

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INPUT_TYPE): cv.one_of(*INPUT_TYPES, lower=True),
        cv.Optional(CONF_INPUT_CHANNELS): cv.one_of(1, 3),
        cv.Optional(CONF_INPUT_WIDTH): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_HEIGHT): cv.int_range(min=8, max=512),
        cv.Optional(CONF_INPUT_ORDER): cv.enum(CHANNEL_ORDERS, upper=True),
        cv.Optional(CONF_NORMALIZE): cv.boolean,
        cv.Optional(CONF_INVERT): cv.boolean,
        cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(ImageInputPipeline),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(ClassificationDecoder),
    }
)


def resolve_image_config(entry: ConfigType, model_path: Path) -> ConfigType:
    """The image configuration: the model's input tensor, then the YAML values."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    if (
        len(model_io.inputs) != 1
        or len(model_io.inputs[0].shape) != 4
        or model_io.inputs[0].shape[0] != 1
    ):
        raise cv.Invalid(
            f"Model '{Path(str(model_path)).name}' does not declare the "
            f"[1, height, width, channels] image input tensor an image model needs"
        )
    tensor = model_io.inputs[0]
    shape = tensor.shape
    if tensor.type not in INPUT_TYPES:
        raise cv.Invalid(
            f"Model '{Path(str(model_path)).name}' declares the input type "
            f"'{tensor.type}', which the image pipeline cannot fill"
        )
    values = {
        CONF_INPUT_TYPE: tensor.type,
        CONF_INPUT_CHANNELS: shape[3],
        CONF_INPUT_WIDTH: shape[2],
        CONF_INPUT_HEIGHT: shape[1],
        CONF_INPUT_ORDER: "GRAY" if shape[3] == 1 else "RGB",
        CONF_NORMALIZE: False,
        CONF_INVERT: False,
    }
    for key in values:
        if (value := entry.get(key)) is not None:
            values[key] = value
    if values[CONF_INPUT_ORDER] == "GRAY" and values[CONF_INPUT_CHANNELS] != 1:
        raise cv.Invalid(
            "input_order: GRAY needs input_channels: 1; a three-channel model has a "
            "colour order to apply"
        )
    return values


def build_pipeline(config: ConfigType, model_path: Path):
    """Create the frame pipeline a kind that consumes images reuses."""
    values = resolve_image_config(config, model_path)
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(
        pipeline.set_geometry(
            values[CONF_INPUT_WIDTH],
            values[CONF_INPUT_HEIGHT],
            values[CONF_INPUT_CHANNELS],
        )
    )
    cg.add(pipeline.set_channel_order(CHANNEL_ORDERS[values[CONF_INPUT_ORDER]]))
    cg.add(pipeline.set_normalize(values[CONF_NORMALIZE]))
    cg.add(pipeline.set_invert(values[CONF_INVERT]))
    return pipeline


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the image pipeline and the classification decoder for one model."""
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(
        config[CONF_DECODER_ID],
        OutputProcessing.OUTPUT_PROCESSING_ARGMAX,
        1.0,
        config.get(CONF_DEBUG, False),
    )
    cg.add(var.set_output_decoder(decoder))
