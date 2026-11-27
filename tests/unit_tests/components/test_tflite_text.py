"""Tests for the tflite_text vocabulary."""

from pathlib import Path

import pytest

from esphome.components import tflite_text
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
