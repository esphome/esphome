"""Tests for the tflite_ctc model and option check."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import tflite_ctc
from esphome.components.tflite_model_io.model_reader import ModelIo, TensorSpec
import esphome.config_validation as cv
from esphome.types import ConfigType


def validate(config: ConfigType, *outputs: TensorSpec) -> tuple[int, int]:
    model_io = ModelIo(inputs=(), outputs=outputs)
    with patch.object(tflite_ctc, "read_model_io", return_value=model_io):
        return tflite_ctc.validate_model_io(Path("speech.tflite"), config)


SCORES = TensorSpec((1, 49, 4), "int8")


def test_the_blank_defaults_to_the_last_class() -> None:
    assert validate({}, SCORES) == (49, 3)


def test_a_configured_blank_and_labels_are_accepted() -> None:
    config = {"blank_index": 0, "labels": ["a", "b", "c"]}

    assert validate(config, SCORES) == (49, 0)


def test_a_blank_outside_the_classes_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="blank_index is 4"):
        validate({"blank_index": 4}, SCORES)


def test_labels_must_cover_every_class_but_the_blank() -> None:
    with pytest.raises(cv.Invalid, match="each of the 3 classes"):
        validate({"labels": ["a", "b", "c", "d"]}, SCORES)


@pytest.mark.parametrize(
    "output",
    (
        TensorSpec((1, 4), "float32"),
        TensorSpec((2, 49, 4), "float32"),
        TensorSpec((1, 49, 1), "float32"),
        TensorSpec((1, 49, 4), "int32"),
    ),
)
def test_other_outputs_are_rejected(output: TensorSpec) -> None:
    with pytest.raises(cv.Invalid, match="declares the output"):
        validate({}, output)


def test_a_model_with_several_outputs_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="2 output tensors"):
        validate({}, SCORES, SCORES)
