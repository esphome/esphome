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
    SOURCE_CLIENT,
    _make_ifdef_line,
    build_message_type,
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
INT32 = descriptor_pb2.FieldDescriptorProto.TYPE_INT32
SINT32 = descriptor_pb2.FieldDescriptorProto.TYPE_SINT32
FIXED64 = descriptor_pb2.FieldDescriptorProto.TYPE_FIXED64
FIXED32 = descriptor_pb2.FieldDescriptorProto.TYPE_FIXED32
FLOAT = descriptor_pb2.FieldDescriptorProto.TYPE_FLOAT
BOOL = descriptor_pb2.FieldDescriptorProto.TYPE_BOOL
STRING = descriptor_pb2.FieldDescriptorProto.TYPE_STRING
BYTES = descriptor_pb2.FieldDescriptorProto.TYPE_BYTES


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


def _field(
    field_type: int, number: int = 1, *, force: bool = False, repeated: bool = False
) -> descriptor_pb2.FieldDescriptorProto:
    field = descriptor_pb2.FieldDescriptorProto(
        name="value", number=number, type=field_type
    )
    if repeated:
        field.label = descriptor_pb2.FieldDescriptorProto.LABEL_REPEATED
    if force:
        field.options.Extensions[pb.force] = True
    return field


def _encode_field(
    field_type: int, number: int = 1, force: bool = False, repeated: bool = False
) -> str:
    """Return the encode statement the generator emits for one encode-only field."""
    field = _field(field_type, number, force=force, repeated=repeated)
    return create_field_type_info(
        field, needs_decode=False, needs_encode=True
    ).encode_content


SCALAR_TYPES = [
    BOOL,
    UINT32,
    INT32,
    UINT64,
    INT64,
    SINT32,
    FLOAT,
    FIXED32,
    STRING,
    BYTES,
]


@pytest.mark.parametrize("field_type", SCALAR_TYPES)
def test_forced_fields_use_the_force_overload_or_raw_writes(field_type: int) -> None:
    content = _encode_field(field_type, force=True)
    assert (
        "_force(" in content
        or "write_raw_byte(" in content
        or "write_tag_and_fixed32(" in content
    ), content


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


def _decode_cases(field_type: int, number: int) -> list[str]:
    """Return the decode_field() case lines the generator emits for one decoded field."""
    field = descriptor_pb2.FieldDescriptorProto(
        name="value", number=number, type=field_type
    )
    ti = create_field_type_info(field, needs_decode=True, needs_encode=False)
    return [
        case
        for case in (
            ti.decode_varint_content,
            ti.decode_length_content,
            ti.decode_32bit_content,
        )
        if case
    ]


UINT32_T = descriptor_pb2.FieldDescriptorProto.TYPE_UINT32
STRING_T = descriptor_pb2.FieldDescriptorProto.TYPE_STRING
BOOL_T = descriptor_pb2.FieldDescriptorProto.TYPE_BOOL


@pytest.mark.parametrize(
    ("field_type", "number", "wire_type", "accessor"),
    [
        (UINT32_T, 2, 0, "value.as_varint()"),
        (BOOL_T, 3, 0, "value.as_varint() != 0"),
        (STRING_T, 1, 2, "value.data()"),
        (FLOAT, 4, 5, "value.as_float()"),
        (FIXED32, 5, 5, "value.as_fixed32()"),
    ],
)
def test_decode_cases_carry_field_number_and_wire_type(
    field_type: int, number: int, wire_type: int, accessor: str
) -> None:
    """Each decoded field yields one case keyed on its number and declared wire type."""
    cases = _decode_cases(field_type, number)
    assert len(cases) == 1, cases
    lines = cases[0].splitlines()
    assert lines[0] == f"case PROTO_DECODE_CASE({number}, {wire_type}):", cases[0]
    assert lines[1].strip() == f"PROTO_DECODE_GUARD(tag, {number}, {wire_type});", (
        cases[0]
    )
    assert accessor in cases[0], cases[0]


def test_message_gets_a_single_decode_field_override() -> None:
    """All wire types of a decoded message land in one decode_field() switch."""
    desc = descriptor_pb2.DescriptorProto(name="Mixed")
    desc.field.add(name="name", number=1, type=STRING_T)
    desc.field.add(name="count", number=2, type=UINT32_T)
    desc.field.add(name="level", number=3, type=FLOAT)
    header, cpp, _ = build_message_type(desc, {}, {"Mixed": SOURCE_CLIENT})
    decl = "bool decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) override;"
    assert header.count(decl) == 1
    assert "decode_varint" not in header and "decode_length" not in header
    assert (
        cpp.count(
            "bool Mixed::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {"
        )
        == 1
    )
    assert "switch (PROTO_DECODE_KEY(tag)) {" in cpp
    assert "const ProtoFieldValue value(data, scalar);" in cpp
    for number, wire_type in ((1, 2), (2, 0), (3, 5)):
        assert f"case PROTO_DECODE_CASE({number}, {wire_type}):" in cpp, cpp


def test_multi_statement_decode_cases_are_scoped() -> None:
    """Bodies with several statements or locals get their own block so no jump crosses an initialization."""
    bytes_type = descriptor_pb2.FieldDescriptorProto.TYPE_BYTES
    cases = _decode_cases(bytes_type, 4)
    assert len(cases) == 1, cases
    lines = cases[0].splitlines()
    assert lines[0] == "case PROTO_DECODE_CASE(4, 2): {", cases[0]
    assert lines[-1] == "}", cases[0]
    assert "value.data();" in cases[0] and "value.size();" in cases[0]
