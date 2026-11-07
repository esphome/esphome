"""Unit tests for script/api_protobuf/api_protobuf.py generator logic.

ci-api-proto.yml only checks that the committed output matches what the
generator currently produces, so a semantic regression in the generator would
be committed and matched without anything failing. These tests pin the
semantics directly.
"""

from __future__ import annotations

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).parents[4] / "script" / "api_protobuf"))

from api_protobuf import (  # noqa: E402
    MAX_MESSAGE_ID,
    _make_ifdef_line,
    get_varint64_ifdef,
    validate_message_id,
)
from google.protobuf import descriptor_pb2  # noqa: E402


def _file_with_messages(
    *messages: tuple[str, int, bool],
) -> descriptor_pb2.FileDescriptorProto:
    """Build a FileDescriptorProto with one single-field message per entry.

    Each entry is (message_name, field_type, deprecated).
    """
    file_desc = descriptor_pb2.FileDescriptorProto(name="test.proto")
    for name, field_type, deprecated in messages:
        msg = file_desc.message_type.add(name=name)
        field = msg.field.add(name="value", number=1, type=field_type)
        field.options.deprecated = deprecated
    return file_desc


UINT64 = descriptor_pb2.FieldDescriptorProto.TYPE_UINT64
INT64 = descriptor_pb2.FieldDescriptorProto.TYPE_INT64
SINT64 = descriptor_pb2.FieldDescriptorProto.TYPE_SINT64
UINT32 = descriptor_pb2.FieldDescriptorProto.TYPE_UINT32
FIXED64 = descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64


def test_no_varint64_fields() -> None:
    file_desc = _file_with_messages(("A", UINT32, False), ("B", FIXED64, False))
    assert get_varint64_ifdef(file_desc, {}) == (False, None)


@pytest.mark.parametrize("field_type", [UINT64, INT64, SINT64])
def test_single_guard_is_kept(field_type: int) -> None:
    file_desc = _file_with_messages(("A", field_type, False))
    assert get_varint64_ifdef(file_desc, {"A": "USE_X"}) == (True, "USE_X")


def test_two_guards_emit_the_union() -> None:
    # The regression this pins: multiple guards used to collapse to
    # unconditional, pulling 64-bit varint support into unrelated builds.
    file_desc = _file_with_messages(("A", UINT64, False), ("B", INT64, False))
    guards = {"A": "USE_X", "B": "USE_Y"}
    assert get_varint64_ifdef(file_desc, guards) == (True, "USE_X || USE_Y")


def test_union_is_sorted_for_deterministic_output() -> None:
    file_desc = _file_with_messages(("B", UINT64, False), ("A", INT64, False))
    guards = {"B": "USE_Y", "A": "USE_X"}
    assert get_varint64_ifdef(file_desc, guards) == (True, "USE_X || USE_Y")


def test_any_unconditional_message_wins() -> None:
    file_desc = _file_with_messages(("A", UINT64, False), ("B", INT64, False))
    assert get_varint64_ifdef(file_desc, {"A": "USE_X"}) == (True, None)


def test_deprecated_fields_and_messages_are_ignored() -> None:
    file_desc = _file_with_messages(("A", UINT64, True), ("B", INT64, False))
    file_desc.message_type[1].options.deprecated = True
    assert get_varint64_ifdef(file_desc, {"A": "USE_X", "B": "USE_Y"}) == (False, None)


def test_make_ifdef_line_simple_identifier() -> None:
    assert _make_ifdef_line("USE_X") == "#ifdef USE_X"


def test_make_ifdef_line_union_wraps_each_identifier() -> None:
    # The second half of the varint64 union guard: compound conditions must
    # become #if defined(A) || defined(B), never #ifdef of the raw string.
    assert _make_ifdef_line("USE_X || USE_Y") == "#if defined(USE_X) || defined(USE_Y)"


def test_make_ifdef_line_conjunction_and_negation() -> None:
    assert (
        _make_ifdef_line("USE_X && !USE_Y") == "#if defined(USE_X) && !defined(USE_Y)"
    )


def test_message_id_at_maximum_is_accepted() -> None:
    # 16383 is the largest ID whose plaintext type varint fits the 2 bytes
    # budgeted in HEADER_PADDING.
    validate_message_id(MAX_MESSAGE_ID, "MaxMessage")


def test_message_id_above_maximum_is_rejected() -> None:
    with pytest.raises(ValueError, match="exceeds the plaintext"):
        validate_message_id(MAX_MESSAGE_ID + 1, "TooBigMessage")
