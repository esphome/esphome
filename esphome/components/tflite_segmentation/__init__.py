"""Pixel segmentation pipeline for tflite_micro_runtime.

The model's single output is a [1, height, width, classes] mask. The frame
preprocessing is the image pipeline from tflite_image; this kind adds the decoder
that turns the mask into one class id per pixel.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_image import IMAGE_SCHEMA, build_pipeline
from esphome.components.tflite_micro_runtime import (
    PER_MODEL_SCHEMA,
    declare_model,
    final_validate,
)
from esphome.components.tflite_model_io.model_reader import read_model_io
from esphome.components.tflite_output_processor import CONF_DECODER_ID
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_segmentation"

# The library components are loaded for the model; only this one appears in the YAML.
AUTO_LOAD = ["tflite_image", "tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

tflite_segmentation_ns = cg.esphome_ns.namespace("tflite_segmentation")
SegmentationDecoder = tflite_segmentation_ns.class_("SegmentationDecoder")


SEGMENTATION_SCHEMA = IMAGE_SCHEMA.extend(
    {cv.GenerateID(CONF_DECODER_ID): cv.declare_id(SegmentationDecoder)}
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(SEGMENTATION_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path) -> None:
    """Reject a model whose mask the decoder cannot read."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"segmentation model needs one mask"
        )
    shape = model_io.outputs[0].shape
    if (
        len(shape) != 4
        or shape[0] != 1
        or shape[1] <= 0
        or shape[2] <= 0
        or not 1 <= shape[3] <= 256
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the mask tensor {list(shape)}; the decoder "
            f"needs [1, height, width, classes] with up to 256 classes"
        )


async def configure(var: cg.MockObj, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the mask decoder for one model."""
    validate_model_io(model_path)
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
