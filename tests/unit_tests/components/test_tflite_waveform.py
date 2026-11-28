"""Tests for the tflite_waveform model check."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import tflite_waveform
from esphome.components.tflite_model_io.model_reader import ModelIo, TensorSpec
import esphome.config_validation as cv


def validate(inputs: list[TensorSpec], outputs: list[TensorSpec]) -> None:
    model_io = ModelIo(inputs=tuple(inputs), outputs=tuple(outputs))
    with patch.object(tflite_waveform, "read_model_io", return_value=model_io):
        tflite_waveform.validate_model_io(Path("denoise.tflite"))


@pytest.mark.parametrize("shape", ((1, 256), (1, 256, 1)))
@pytest.mark.parametrize("tensor_type", ("float32", "int8", "uint8"))
def test_a_waveform_model_is_accepted(shape: tuple[int, ...], tensor_type: str) -> None:
    spec = TensorSpec(shape, tensor_type)

    validate([spec], [spec])


@pytest.mark.parametrize(
    "spec",
    (
        TensorSpec((1, 256), "int16"),
        TensorSpec((2, 256), "float32"),
        TensorSpec((1, 49, 40), "float32"),
        TensorSpec((256,), "float32"),
        TensorSpec((1, 0), "float32"),
    ),
)
@pytest.mark.parametrize("role", ("input", "output"))
def test_other_tensors_are_rejected(spec: TensorSpec, role: str) -> None:
    good = TensorSpec((1, 256), "float32")
    inputs, outputs = ([spec], [good]) if role == "input" else ([good], [spec])

    with pytest.raises(cv.Invalid, match=f"declares the {role}"):
        validate(inputs, outputs)


def test_a_model_with_state_tensors_is_rejected() -> None:
    spec = TensorSpec((1, 256), "float32")

    with pytest.raises(cv.Invalid, match="2 input tensors"):
        validate([spec, spec], [spec])
