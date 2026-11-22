"""Pixel segmentation pipeline for tflite_micro_runtime.

The model's single output is a [1, height, width, classes] mask. The frame
preprocessing is the image pipeline from tflite_image; this kind adds the decoder
that turns the mask into one class id per pixel.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_image import IMAGE_SCHEMA, build_pipeline
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_segmentation"

tflite_segmentation_ns = cg.esphome_ns.namespace("tflite_segmentation")
SegmentationDecoder = tflite_segmentation_ns.class_("SegmentationDecoder")

SEGMENTATION_SCHEMA = IMAGE_SCHEMA

# Library component: the runtime creates the pipeline from its own entry.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(var, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the mask decoder for one model."""
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(
        ID(
            f"{config[CONF_ID].id}_segmentation",
            is_declaration=True,
            type=SegmentationDecoder,
        )
    )
    cg.add(var.set_output_decoder(decoder))
