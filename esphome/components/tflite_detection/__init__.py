"""Object detection pipeline for tflite_micro_runtime.

The model's four output tensors are boxes, classes, scores and the detection count.
The frame preprocessing is the image pipeline from tflite_image; this kind adds the
detection keys and the decoder.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components.tflite_image import IMAGE_SCHEMA, build_pipeline
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "tflite_detection"

CONF_SCORE_THRESHOLD = "score_threshold"
CONF_MAX_DETECTIONS = "max_detections"
CONF_IOU_THRESHOLD = "iou_threshold"

tflite_detection_ns = cg.esphome_ns.namespace("tflite_detection")
DetectionDecoder = tflite_detection_ns.class_("DetectionDecoder")

DETECTION_SCHEMA = IMAGE_SCHEMA.extend(
    {
        cv.Optional(CONF_SCORE_THRESHOLD, default=0.5): cv.percentage,
        cv.Optional(CONF_MAX_DETECTIONS, default=10): cv.int_range(min=1, max=64),
        cv.Optional(CONF_IOU_THRESHOLD, default=0.5): cv.percentage,
    }
)

# Library component: the runtime creates the pipeline from its own entry.
CONFIG_SCHEMA = cv.Schema({})


async def configure(var, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the detection decoder for one model."""
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(
        ID(
            f"{config[CONF_ID].id}_detection",
            is_declaration=True,
            type=DetectionDecoder,
        ),
        config[CONF_SCORE_THRESHOLD],
        config[CONF_MAX_DETECTIONS],
        config[CONF_IOU_THRESHOLD],
    )
    cg.add(var.set_output_decoder(decoder))
