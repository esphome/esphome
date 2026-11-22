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

CONF_DECODER_ID = "decoder_id"

SEGMENTATION_SCHEMA = IMAGE_SCHEMA.extend(
    {cv.GenerateID(CONF_DECODER_ID): cv.declare_id(SegmentationDecoder)}
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(SEGMENTATION_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the mask decoder for one model."""
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
