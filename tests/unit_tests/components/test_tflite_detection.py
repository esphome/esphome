"""Tests for the tflite_detection model check."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import tflite_detection
from esphome.components.tflite_model_io.model_reader import ModelIo, TensorSpec
import esphome.config_validation as cv


def validate(output_format: str, *shapes: tuple[int, ...]) -> None:
    outputs = tuple(TensorSpec(shape, "float32") for shape in shapes)
    model_io = ModelIo(inputs=(), outputs=outputs)
    with patch.object(tflite_detection, "read_model_io", return_value=model_io):
        tflite_detection.validate_model_io(Path("detector.tflite"), output_format)


@pytest.mark.parametrize(
    ("output_format", "shape"),
    (("yolov5", (1, 6300, 85)), ("yolov8", (1, 84, 2100))),
)
def test_a_yolo_output_is_accepted(output_format: str, shape: tuple[int, ...]) -> None:
    validate(output_format, shape)


@pytest.mark.parametrize(
    ("output_format", "shape"),
    (
        ("yolov5", (1, 6300, 5)),
        ("yolov8", (1, 4, 2100)),
        ("yolov5", (1, 85)),
        ("yolov8", (2, 84, 2100)),
    ),
)
def test_other_yolo_outputs_are_rejected(
    output_format: str, shape: tuple[int, ...]
) -> None:
    with pytest.raises(cv.Invalid, match="needs"):
        validate(output_format, shape)


def test_a_yolo_model_has_one_output() -> None:
    with pytest.raises(cv.Invalid, match="4 output tensors"):
        validate("yolov5", *[(1, 10, 6)] * 4)


def test_ssd_stays_the_default() -> None:
    with pytest.raises(cv.Invalid, match="boxes, classes, scores"):
        validate("ssd", (1, 6300, 85))
