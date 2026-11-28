"""Tests for the tflite_text vocabulary."""

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components import tflite_text
from esphome.components.tflite_model_io.model_reader import ModelIo, TensorSpec
import esphome.config_validation as cv

SPECIAL = b"[PAD]\n[UNK]\n[CLS]\n[SEP]\n"


def load(tmp_path: Path, data: bytes) -> list[bytes]:
    path = tmp_path / "vocab.txt"
    path.write_bytes(data)
    return tflite_text.load_vocabulary(path)


@pytest.mark.parametrize("newline", (b"\n", b"\r\n"))
def test_tokens_are_read_in_id_order(tmp_path: Path, newline: bytes) -> None:
    data = (SPECIAL + "caf\u00e9\n##s\n".encode()).replace(b"\n", newline)

    assert load(tmp_path, data)[4:] == ["caf\u00e9".encode(), b"##s"]


@pytest.mark.parametrize(
    ("data", "message"),
    (
        (b"", "1 to 65536 tokens"),
        (SPECIAL + b"a\n\nb\n", "Line 6"),
        (SPECIAL + b"a\na\n", "Line 6"),
        (b"[PAD]\n[UNK]\n", r"lacks \[CLS\], \[SEP\]"),
        (SPECIAL + b"\xff\n", "Cannot read"),
    ),
)
def test_a_bad_vocabulary_is_rejected(
    tmp_path: Path, data: bytes, message: str
) -> None:
    with pytest.raises(cv.Invalid, match=message):
        load(tmp_path, data)


def validate_output(output_type: str) -> None:
    model_io = ModelIo(
        inputs=(TensorSpec((1, 16), "int32"),),
        outputs=(TensorSpec((1, 2), output_type),),
    )
    with patch.object(tflite_text, "read_model_io", return_value=model_io):
        tflite_text.validate_model_io(Path("text.tflite"))


@pytest.mark.parametrize("output_type", ("float32", "uint8", "int8", "int32"))
def test_a_readable_output_type_is_accepted(output_type: str) -> None:
    validate_output(output_type)


@pytest.mark.parametrize("output_type", ("int64", "int16", "bool"))
def test_an_unreadable_output_type_is_rejected(output_type: str) -> None:
    with pytest.raises(cv.Invalid, match=f"declares a {output_type} output"):
        validate_output(output_type)
