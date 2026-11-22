"""Pose estimation pipeline for tflite_micro_runtime.

The model's single output is a [1, 1, K, 3] tensor of (y, x, score) keypoints.
The frame preprocessing is the image pipeline from tflite_image.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_image import IMAGE_SCHEMA, build_pipeline
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_pose"

# The frame pipeline comes from tflite_image, so its sources must be built too.
DEPENDENCIES = ["tflite_image"]

tflite_pose_ns = cg.esphome_ns.namespace("tflite_pose")
PoseDecoder = tflite_pose_ns.class_("PoseDecoder")

CONF_DECODER_ID = "decoder_id"

POSE_SCHEMA = IMAGE_SCHEMA.extend(
    {cv.GenerateID(CONF_DECODER_ID): cv.declare_id(PoseDecoder)}
)

# Library component: the runtime creates the pipeline from its own entry.
CONFIG_SCHEMA = cv.Schema({})


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the keypoint decoder for one model."""
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))
