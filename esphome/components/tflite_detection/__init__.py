"""Object detection pipeline for tflite_micro_runtime.

An SSD model's four output tensors are boxes, classes, scores and the detection count;
a YOLO model has one tensor of raw candidates, selected with ``output_format``. The
frame preprocessing is the image pipeline from tflite_image; this kind adds the
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
from esphome.components.tflite_model_io.model_reader import ModelIo, read_model_io
from esphome.components.tflite_output_processor import (
    CONF_DECODER_ID,
    CONF_OUTPUT_PROCESSING,
    CONF_SCALE_FACTOR,
)
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
CONF_OUTPUT_FORMAT = "output_format"

# The image schema also carries the classification options. A detection decoder
# does not read them, so they are removed here instead of being accepted and
# silently ignored.

tflite_detection_ns = cg.esphome_ns.namespace("tflite_detection")
DetectionDecoder = tflite_detection_ns.class_("DetectionDecoder")
DetectionFormat = tflite_detection_ns.enum("DetectionFormat", is_class=True)
OUTPUT_FORMATS = {
    "ssd": DetectionFormat.DETECTION_FORMAT_SSD,
    "yolov5": DetectionFormat.DETECTION_FORMAT_YOLOV5,
    "yolov8": DetectionFormat.DETECTION_FORMAT_YOLOV8,
}

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
        cv.Optional(CONF_OUTPUT_FORMAT, default="ssd"): cv.enum(
            OUTPUT_FORMATS, lower=True
        ),
        cv.GenerateID(CONF_DECODER_ID): cv.declare_id(DetectionDecoder),
    }
)

CONFIG_SCHEMA = cv.All(
    PER_MODEL_SCHEMA.extend(DETECTION_SCHEMA).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


def _validate_yolo_output(name: str, model_io: ModelIo, output_format: str) -> None:
    """Reject a model whose single output is not the YOLO candidate matrix."""
    if len(model_io.outputs) != 1:
        raise cv.Invalid(
            f"Model '{name}' declares {len(model_io.outputs)} output tensors; a "
            f"{output_format} model has one"
        )
    output = model_io.outputs[0]
    shape = list(output.shape)
    # yolov5: [1, candidates, 5 + classes]; yolov8: [1, 4 + classes, candidates].
    values_axis, minimum, layout = (
        (2, 6, "[1, candidates, 5 + classes]")
        if output_format == "yolov5"
        else (1, 5, "[1, 4 + classes, candidates]")
    )
    if (
        output.type not in ("float32", "int8", "uint8")
        or len(shape) != 3
        or shape[0] != 1
        or min(shape) <= 0
        or shape[values_axis] < minimum
    ):
        raise cv.Invalid(
            f"Model '{name}' declares the output {output.type} {shape}; "
            f"{output_format} needs {layout}"
        )


def validate_model_io(model_path: Path, output_format: str = "ssd") -> None:
    """Reject a model whose outputs the detection decoder cannot read."""
    model_io = read_model_io(model_path)
    name = Path(str(model_path)).name
    if output_format != "ssd":
        _validate_yolo_output(name, model_io, output_format)
        return
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


async def configure(var: cg.MockObj, config: ConfigType, model_path: Path) -> None:
    """Create the frame pipeline and the detection decoder for one model."""
    validate_model_io(model_path, config[CONF_OUTPUT_FORMAT])
    cg.add(var.set_input_pipeline(build_pipeline(config, model_path)))
    decoder = cg.new_Pvariable(
        config[CONF_DECODER_ID],
        config[CONF_SCORE_THRESHOLD],
        config[CONF_MAX_DETECTIONS],
        config[CONF_IOU_THRESHOLD],
        config[CONF_OUTPUT_FORMAT],
    )
    cg.add(var.set_output_decoder(decoder))


async def to_code(config: ConfigType) -> None:
    """Create the runtime helper from this entry, then the pipeline and decoder."""
    var, model_path = await declare_model(config)
    await configure(var, config, model_path)
