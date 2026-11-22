"""Tests for the model_io arena estimate rules that do not need a model file."""

import pytest

from esphome.components.tflite_model_io.model_reader import (
    TENSOR_TYPE_NAMES,
    ModelReadError,
    _peak_activation_bytes,
    _Tensor,
)


def test_a_subgraph_without_operators_is_an_error() -> None:
    """An empty operator list leaves nothing live, so the estimate would be too small."""
    tensors = [_Tensor(10, True)]

    with pytest.raises(ModelReadError, match="no operators"):
        _peak_activation_bytes(tensors, [], [0], [0])


def test_a_subgraph_output_stays_live_until_the_end() -> None:
    """Two branches that only meet at the outputs must both count at the last op."""
    tensors = [_Tensor(10, True), _Tensor(20, True)]
    operators = [([], [0]), ([], [1])]

    assert _peak_activation_bytes(tensors, operators, [], [0, 1]) == 30


def test_a_subgraph_input_is_live_from_the_start() -> None:
    """An input read by the second operator is still live at the first one."""
    tensors = [_Tensor(10, True), _Tensor(20, True)]
    operators = [([], [0]), ([0], [1])]

    assert _peak_activation_bytes(tensors, operators, [0], [1]) == 30


def test_an_operator_may_read_and_write_the_same_tensor() -> None:
    """One tensor used as both the input and the output is live for a single step."""
    tensors = [_Tensor(10, True)]
    operators = [([0], [0])]

    assert _peak_activation_bytes(tensors, operators, [0], [0]) == 10


def test_the_type_names_cover_every_tflite_tensor_type() -> None:
    """A model with float16 or uint32 tensors must still be described."""
    expected = {
        0: "float32",
        1: "float16",
        2: "int32",
        3: "uint8",
        4: "int64",
        5: "string",
        6: "bool",
        7: "int16",
        8: "complex64",
        9: "int8",
        10: "float64",
        11: "complex128",
        12: "uint64",
        13: "resource",
        14: "variant",
        15: "uint32",
        16: "uint16",
        17: "int4",
        18: "bfloat16",
    }

    assert expected == TENSOR_TYPE_NAMES
