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

import aioesphomeapi.api_options_pb2 as pb  # noqa: E402
from api_protobuf import (  # noqa: E402
    MAX_MESSAGE_ID,
    _make_ifdef_line,
    create_field_type_info,
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


def _encode_field(
    field_type: int, number: int = 1, force: bool = False, repeated: bool = False
) -> str:
    """Return the encode statement the generator emits for one encode-only field."""
    field = descriptor_pb2.FieldDescriptorProto(
        name="value", number=number, type=field_type
    )
    if repeated:
        field.label = descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
    if force:
        field.options.Extensions[pb.force] = True
    ti = create_field_type_info(field, needs_decode=False, needs_encode=True)
    return ti.encode_content


SCALAR_TYPES = [
    descriptor_pb2.FieldDescriptorProto.TYPE_BOOL,
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT32,
    descriptor_pb2.FieldDescriptorProto.TYPE_INT32,
    descriptor_pb2.FieldDescriptorProto.TYPE_UINT64,
    descriptor_pb2.FieldDescriptorProto.TYPE_INT64,
    descriptor_pb2.FieldDescriptorProto.TYPE_SINT32,
    descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT,
    descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32,
    descriptor_pb2.FieldDescriptorProto.TYPE_STRING,
    descriptor_pb2.FieldDescriptorProto.TYPE_BYTES,
]


@pytest.mark.parametrize("field_type", SCALAR_TYPES)
@pytest.mark.parametrize("force", [False, True])
@pytest.mark.parametrize("repeated", [False, True])
def test_encode_statements_assign_the_returned_cursor(
    field_type: int, force: bool, repeated: bool
) -> None:
    """Every ProtoEncode call must take pos by value and store the returned cursor."""
    content = _encode_field(field_type, force=force, repeated=repeated)
    calls = [line.strip() for line in content.splitlines() if "ProtoEncode::" in line]
    assert calls, content
    for call in calls:
        assert call.startswith("pos = ProtoEncode::"), call
    assert ", true)" not in content, content


@pytest.mark.parametrize("field_type", SCALAR_TYPES)
def test_forced_fields_use_the_force_overload_or_raw_writes(field_type: int) -> None:
    content = _encode_field(field_type, force=True)
    assert (
        "_force(" in content
        or "write_raw_byte(" in content
        or "write_tag_and_fixed32(" in content
    ), content


FLOAT = descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT
FIXED32 = descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32


@pytest.mark.parametrize("field_type", [FLOAT, FIXED32])
def test_single_byte_tag_fixed32_shares_the_outlined_writer(field_type: int) -> None:
    unconditional = _encode_field(field_type, force=True)
    assert unconditional.count("write_tag_and_fixed32(pos, 13,") == 1, unconditional
    guarded = _encode_field(field_type, force=False)
    assert guarded.startswith("if ("), guarded
    assert "[[likely]]" in guarded
    assert "write_tag_and_fixed32(pos, 13," in guarded


@pytest.mark.parametrize("field_type", [FLOAT, FIXED32])
def test_multi_byte_tag_fixed32_falls_back_to_the_generic_helper(
    field_type: int,
) -> None:
    content = _encode_field(field_type, number=16)
    assert "write_tag_and_fixed32" not in content, content
    assert content.startswith("pos = ProtoEncode::encode_"), content
