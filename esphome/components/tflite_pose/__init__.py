"""Pose estimation pipeline for tflite_micro_runtime.

The model's single output is a [1, 1, K, 3] tensor of (y, x, score) keypoints.
The frame preprocessing is the image pipeline from tflite_image.
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
from esphome.components.tflite_output_processor import (
    CONF_DECODER_ID,
    CONF_OUTPUT_PROCESSING,
    CONF_SCALE_FACTOR,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_pose"

# The library components are loaded for the model; only this one appears in the YAML.
AUTO_LOAD = ["tflite_image", "tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

tflite_pose_ns = cg.esphome_ns.namespace("tflite_pose")
PoseDecoder = tflite_pose_ns.class_("PoseDecoder")


# The image schema also carries the classification options. This decoder does
# not read them, so they are removed instead of being accepted and ignored.
POSE_SCHEMA = cv.Schema(
    {
        key: value
        for key, value in IMAGE_SCHEMA.schema.items()
        if key not in (CONF_OUTPUT_PROCESSING, CONF_SCALE_FACTOR)
    }
).extend({cv.GenerateID(CONF_DECODER_ID): cv.declare_id(PoseDecoder)})

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(POSE_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path) -> None:
    """Reject a model whose keypoints the decoder cannot read."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    name = Path(str(model_path)).name
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"pose model needs one keypoint tensor"
        )
    shape = model_io.outputs[0].shape
    if (
        len(shape) != 4
        or shape[0] != 1
        or shape[1] != 1
        or shape[2] <= 0
        or shape[3] != 3
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the keypoint tensor {list(shape)}; the "
            f"decoder needs [1, 1, K, 3]"
        )


async def configure(var: cg.MockObj, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the keypoint decoder for one model."""
    validate_model_io(model_path)
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(config[CONF_DECODER_ID])
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
