"""Object detection pipeline for tflite_micro_runtime.

The model's four output tensors are boxes, classes, scores and the detection count.
The frame preprocessing is the image pipeline from tflite_image; this kind adds the
detection keys and the decoder.
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
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_detection"

# The library components are loaded for the model; only this one appears in the YAML.
AUTO_LOAD = ["tflite_image", "tflite_micro_runtime"]
MULTI_CONF = True
FINAL_VALIDATE_SCHEMA = final_validate

CONF_SCORE_THRESHOLD = "score_threshold"
CONF_MAX_DETECTIONS = "max_detections"
CONF_IOU_THRESHOLD = "iou_threshold"
CONF_DECODER_ID = "decoder_id"

# The image schema also carries the classification options. A detection decoder
# does not read them, so they are removed here instead of being accepted and
# silently ignored.
CONF_OUTPUT_PROCESSING = "output_processing"
CONF_SCALE_FACTOR = "scale_factor"

tflite_detection_ns = cg.esphome_ns.namespace("tflite_detection")
DetectionDecoder = tflite_detection_ns.class_("DetectionDecoder")

DETECTION_SCHEMA = cv.Schema(
    {
        key: value
        for key, value in IMAGE_SCHEMA.schema.items()
        if key not in (CONF_OUTPUT_PROCESSING, CONF_SCALE_FACTOR)
    }
).extend(
    {
        cv.Optional(CONF_SCORE_THRESHOLD, default=0.5): cv.percentage,
        cv.Optional(CONF_MAX_DETECTIONS, default=10): cv.int_range(min=1, max=64),
        cv.Optional(CONF_IOU_THRESHOLD, default=0.5): cv.percentage,
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(DetectionDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(DETECTION_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def validate_model_io(model_path: Path) -> None:
    """Reject a model whose outputs the detection decoder cannot read."""
    model_io = read_model_io(model_path)
    name = Path(str(model_path)).name
    if len(model_io.outputs) != 4:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"detection model needs boxes, classes, scores and the detection count"
        )
    boxes = model_io.outputs[0].shape
    if len(boxes) != 3 or boxes[2] != 4:
        raise cv.Invalid(
            f"Model '{name}' declares the boxes tensor {list(boxes)}; the decoder "
            f"needs [1, N, 4]"
        )


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the detection decoder for one model."""
    validate_model_io(model_path)
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(
        config[CONF_DECODER_ID],
        config[CONF_SCORE_THRESHOLD],
        config[CONF_MAX_DETECTIONS],
        config[CONF_IOU_THRESHOLD],
    )
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
