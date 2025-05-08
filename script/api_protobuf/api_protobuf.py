#!/usr/bin/env python3
from __future__ import annotations

from abc import ABC, abstractmethod
from enum import IntEnum
import os
from pathlib import Path
import re
from subprocess import call
import sys
from textwrap import dedent
from typing import Any

import aioesphomeapi.api_options_pb2 as pb
import google.protobuf.descriptor_pb2 as descriptor


class WireType(IntEnum):
    """Protocol Buffer wire types as defined in the protobuf spec.

    As specified in the Protocol Buffers encoding guide:
    https://protobuf.dev/programming-guides/encoding/#structure
    """

    VARINT = 0  # int32, int64, uint32, uint64, sint32, sint64, bool, enum
    FIXED64 = 1  # fixed64, sfixed64, double
    LENGTH_DELIMITED = 2  # string, bytes, embedded messages, packed repeated fields
    START_GROUP = 3  # groups (deprecated)
    END_GROUP = 4  # groups (deprecated)
    FIXED32 = 5  # fixed32, sfixed32, float


# Generate with
# protoc --python_out=script/api_protobuf -I esphome/components/api/ api_options.proto


"""Python 3 script to automatically generate C++ classes for ESPHome's native API.

It's pretty crappy spaghetti code, but it works.

you need to install protobuf-compiler:
running protoc --version should return
libprotoc 3.6.1

then run this script with python3 and the files

    esphome/components/api/api_pb2_service.h
    esphome/components/api/api_pb2_service.cpp
    esphome/components/api/api_pb2.h
    esphome/components/api/api_pb2.cpp

will be generated, they still need to be formatted
"""


FILE_HEADER = """// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
"""


def indent_list(text: str, padding: str = "  ") -> list[str]:
    """Indent each line of the given text with the specified padding."""
    lines = []
    for line in text.splitlines():
        if line == "":
            p = ""
        elif line.startswith("#ifdef") or line.startswith("#endif"):
            p = ""
        else:
            p = padding
        lines.append(p + line)
    return lines


def indent(text: str, padding: str = "  ") -> str:
    return "\n".join(indent_list(text, padding))


def camel_to_snake(name: str) -> str:
    # https://stackoverflow.com/a/1176023
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s1).lower()


def force_str(force: bool) -> str:
    """Convert a boolean force value to string format for C++ code."""
    return str(force).lower()


class TypeInfo(ABC):
    """Base class for all type information."""

    def __init__(self, field: descriptor.FieldDescriptorProto) -> None:
        self._field = field

    @property
    def default_value(self) -> str:
        """Get the default value."""
        return ""

    @property
    def name(self) -> str:
        """Get the name of the field."""
        return self._field.name

    @property
    def arg_name(self) -> str:
        """Get the argument name."""
        return self.name

    @property
    def field_name(self) -> str:
        """Get the field name."""
        return self.name

    @property
    def number(self) -> int:
        """Get the field number."""
        return self._field.number

    @property
    def repeated(self) -> bool:
        """Check if the field is repeated."""
        return self._field.label == 3

    @property
    def wire_type(self) -> WireType:
        """Get the wire type for the field."""
        raise NotImplementedError

    @property
    def cpp_type(self) -> str:
        raise NotImplementedError

    @property
    def reference_type(self) -> str:
        return f"{self.cpp_type} "

    @property
    def const_reference_type(self) -> str:
        return f"{self.cpp_type} "

    @property
    def public_content(self) -> str:
        return [self.class_member]

    @property
    def protected_content(self) -> str:
        return []

    @property
    def class_member(self) -> str:
        return f"{self.cpp_type} {self.field_name}{{{self.default_value}}};"

    @property
    def decode_varint_content(self) -> str:
        content = self.decode_varint
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name} = {content};
          return true;
        }}"""
        )

    decode_varint = None

    @property
    def decode_length_content(self) -> str:
        content = self.decode_length
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name} = {content};
          return true;
        }}"""
        )

    decode_length = None

    @property
    def decode_32bit_content(self) -> str:
        content = self.decode_32bit
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name} = {content};
          return true;
        }}"""
        )

    decode_32bit = None

    @property
    def decode_64bit_content(self) -> str:
        content = self.decode_64bit
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name} = {content};
          return true;
        }}"""
        )

    decode_64bit = None

    @property
    def encode_content(self) -> str:
        return f"buffer.{self.encode_func}({self.number}, this->{self.field_name});"

    encode_func = None

    @property
    def dump_content(self) -> str:
        o = f'out.append("  {self.name}: ");\n'
        o += self.dump(f"this->{self.field_name}") + "\n"
        o += 'out.append("\\n");\n'
        return o

    @abstractmethod
    def dump(self, name: str) -> str:
        """Dump the value to the output."""

    def calculate_field_id_size(self) -> int:
        """Calculates the size of a field ID in bytes.

        Returns:
            The number of bytes needed to encode the field ID
        """
        # Calculate the tag by combining field_id and wire_type
        tag = (self.number << 3) | (self.wire_type & 0b111)

        # Calculate the varint size
        if tag < 128:
            return 1  # 7 bits
        if tag < 16384:
            return 2  # 14 bits
        if tag < 2097152:
            return 3  # 21 bits
        if tag < 268435456:
            return 4  # 28 bits
        return 5  # 32 bits (maximum for uint32_t)

    @abstractmethod
    def get_size_calculation(self, name: str, force: bool = False) -> str:
        """Calculate the size needed for encoding this field.

        Args:
            name: The name of the field
            force: Whether to force encoding the field even if it has a default value
        """


TYPE_INFO: dict[int, TypeInfo] = {}


def register_type(name: int):
    """Decorator to register a type with a name and number."""

    def func(value: TypeInfo) -> TypeInfo:
        """Register the type with the given name and number."""
        TYPE_INFO[name] = value
        return value

    return func


@register_type(1)
class DoubleType(TypeInfo):
    cpp_type = "double"
    default_value = "0.0"
    decode_64bit = "value.as_double()"
    encode_func = "encode_double"
    wire_type = WireType.FIXED64  # Uses wire type 1 according to protobuf spec

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%g", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<8>(total_size, {field_id_size}, {name} != 0.0, {force_str(force)});"
        return o


@register_type(2)
class FloatType(TypeInfo):
    cpp_type = "float"
    default_value = "0.0f"
    decode_32bit = "value.as_float()"
    encode_func = "encode_float"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%g", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<4>(total_size, {field_id_size}, {name} != 0.0f, {force_str(force)});"
        return o


@register_type(3)
class Int64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_int64()"
    encode_func = "encode_int64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_int64_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(4)
class UInt64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_varint = "value.as_uint64()"
    encode_func = "encode_uint64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%llu", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_uint64_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(5)
class Int32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_int32()"
    encode_func = "encode_int32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_int32_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(6)
class Fixed64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_64bit = "value.as_fixed64()"
    encode_func = "encode_fixed64"
    wire_type = WireType.FIXED64  # Uses wire type 1

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%llu", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<8>(total_size, {field_id_size}, {name} != 0, {force_str(force)});"
        return o


@register_type(7)
class Fixed32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_32bit = "value.as_fixed32()"
    encode_func = "encode_fixed32"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%" PRIu32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<4>(total_size, {field_id_size}, {name} != 0, {force_str(force)});"
        return o


@register_type(8)
class BoolType(TypeInfo):
    cpp_type = "bool"
    default_value = "false"
    decode_varint = "value.as_bool()"
    encode_func = "encode_bool"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f"out.append(YESNO({name}));"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_bool_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(9)
class StringType(TypeInfo):
    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    decode_length = "value.as_string()"
    encode_func = "encode_string"
    wire_type = WireType.LENGTH_DELIMITED  # Uses wire type 2

    def dump(self, name):
        o = f'out.append("\'").append({name}).append("\'");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_string_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(11)
class MessageType(TypeInfo):
    @property
    def cpp_type(self) -> str:
        return self._field.type_name[1:]

    default_value = ""
    wire_type = WireType.LENGTH_DELIMITED  # Uses wire type 2

    @property
    def reference_type(self) -> str:
        return f"{self.cpp_type} &"

    @property
    def const_reference_type(self) -> str:
        return f"const {self.cpp_type} &"

    @property
    def encode_func(self) -> str:
        return f"encode_message<{self.cpp_type}>"

    @property
    def decode_length(self) -> str:
        return f"value.as_message<{self.cpp_type}>()"

    def dump(self, name: str) -> str:
        o = f"{name}.dump_to(out);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_message_object(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(12)
class BytesType(TypeInfo):
    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    decode_length = "value.as_string()"
    encode_func = "encode_string"
    wire_type = WireType.LENGTH_DELIMITED  # Uses wire type 2

    def dump(self, name: str) -> str:
        o = f'out.append("\'").append({name}).append("\'");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_string_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(13)
class UInt32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_varint = "value.as_uint32()"
    encode_func = "encode_uint32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%" PRIu32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_uint32_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(14)
class EnumType(TypeInfo):
    @property
    def cpp_type(self) -> str:
        return f"enums::{self._field.type_name[1:]}"

    @property
    def decode_varint(self) -> str:
        return f"value.as_enum<{self.cpp_type}>()"

    default_value = ""
    wire_type = WireType.VARINT  # Uses wire type 0

    @property
    def encode_func(self) -> str:
        return f"encode_enum<{self.cpp_type}>"

    def dump(self, name: str) -> str:
        o = f"out.append(proto_enum_to_string<{self.cpp_type}>({name}));"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_enum_field(total_size, {field_id_size}, static_cast<uint32_t>({name}), {force_str(force)});"
        return o


@register_type(15)
class SFixed32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_32bit = "value.as_sfixed32()"
    encode_func = "encode_sfixed32"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<4>(total_size, {field_id_size}, {name} != 0, {force_str(force)});"
        return o


@register_type(16)
class SFixed64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_64bit = "value.as_sfixed64()"
    encode_func = "encode_sfixed64"
    wire_type = WireType.FIXED64  # Uses wire type 1

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_fixed_field<8>(total_size, {field_id_size}, {name} != 0, {force_str(force)});"
        return o


@register_type(17)
class SInt32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_sint32()"
    encode_func = "encode_sint32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_sint32_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


@register_type(18)
class SInt64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_sint64()"
    encode_func = "encode_sint64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        o = f"ProtoSize::add_sint64_field(total_size, {field_id_size}, {name}, {force_str(force)});"
        return o


class RepeatedTypeInfo(TypeInfo):
    def __init__(self, field: descriptor.FieldDescriptorProto) -> None:
        super().__init__(field)
        self._ti: TypeInfo = TYPE_INFO[field.type](field)

    @property
    def cpp_type(self) -> str:
        return f"std::vector<{self._ti.cpp_type}>"

    @property
    def reference_type(self) -> str:
        return f"{self.cpp_type} &"

    @property
    def const_reference_type(self) -> str:
        return f"const {self.cpp_type} &"

    @property
    def wire_type(self) -> WireType:
        """Get the wire type for this repeated field.

        For repeated fields, we use the same wire type as the underlying field.
        """
        return self._ti.wire_type

    @property
    def decode_varint_content(self) -> str:
        content = self._ti.decode_varint
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name}.push_back({content});
          return true;
        }}"""
        )

    @property
    def decode_length_content(self) -> str:
        content = self._ti.decode_length
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name}.push_back({content});
          return true;
        }}"""
        )

    @property
    def decode_32bit_content(self) -> str:
        content = self._ti.decode_32bit
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name}.push_back({content});
          return true;
        }}"""
        )

    @property
    def decode_64bit_content(self) -> str:
        content = self._ti.decode_64bit
        if content is None:
            return None
        return dedent(
            f"""\
        case {self.number}: {{
          this->{self.field_name}.push_back({content});
          return true;
        }}"""
        )

    @property
    def _ti_is_bool(self) -> bool:
        # std::vector is specialized for bool, reference does not work
        return isinstance(self._ti, BoolType)

    @property
    def encode_content(self) -> str:
        o = f"for (auto {'' if self._ti_is_bool else '&'}it : this->{self.field_name}) {{\n"
        o += f"  buffer.{self._ti.encode_func}({self.number}, it, true);\n"
        o += "}"
        return o

    @property
    def dump_content(self) -> str:
        o = f"for (const auto {'' if self._ti_is_bool else '&'}it : this->{self.field_name}) {{\n"
        o += f'  out.append("  {self.name}: ");\n'
        o += indent(self._ti.dump("it")) + "\n"
        o += '  out.append("\\n");\n'
        o += "}\n"
        return o

    def dump(self, _: str):
        pass

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # For repeated fields, we always need to pass force=True to the underlying type's calculation
        # This is because the encode method always sets force=true for repeated fields
        if isinstance(self._ti, MessageType):
            # For repeated messages, use the dedicated helper that handles iteration internally
            field_id_size = self._ti.calculate_field_id_size()
            o = f"ProtoSize::add_repeated_message(total_size, {field_id_size}, {name});"
            return o
        # For other repeated types, use the underlying type's size calculation with force=True
        o = f"if (!{name}.empty()) {{\n"
        o += f"  for (const auto {'' if self._ti_is_bool else '&'}it : {name}) {{\n"
        o += f"    {self._ti.get_size_calculation('it', True)}\n"
        o += "  }\n"
        o += "}"
        return o


def build_enum_type(desc) -> tuple[str, str]:
    """Builds the enum type."""
    name = desc.name
    out = f"enum {name} : uint32_t {{\n"
    for v in desc.value:
        out += f"  {v.name} = {v.number},\n"
    out += "};\n"

    cpp = "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    cpp += f"template<> const char *proto_enum_to_string<enums::{name}>(enums::{name} value) {{\n"
    cpp += "  switch (value) {\n"
    for v in desc.value:
        cpp += f"    case enums::{v.name}:\n"
        cpp += f'      return "{v.name}";\n'
    cpp += "    default:\n"
    cpp += '      return "UNKNOWN";\n'
    cpp += "  }\n"
    cpp += "}\n"
    cpp += "#endif\n"

    return out, cpp


def build_message_type(desc: descriptor.DescriptorProto) -> tuple[str, str]:
    public_content: list[str] = []
    protected_content: list[str] = []
    decode_varint: list[str] = []
    decode_length: list[str] = []
    decode_32bit: list[str] = []
    decode_64bit: list[str] = []
    encode: list[str] = []
    dump: list[str] = []
    size_calc: list[str] = []

    for field in desc.field:
        if field.label == 3:
            ti = RepeatedTypeInfo(field)
        else:
            ti = TYPE_INFO[field.type](field)
        protected_content.extend(ti.protected_content)
        public_content.extend(ti.public_content)
        encode.append(ti.encode_content)
        size_calc.append(ti.get_size_calculation(f"this->{ti.field_name}"))

        if ti.decode_varint_content:
            decode_varint.append(ti.decode_varint_content)
        if ti.decode_length_content:
            decode_length.append(ti.decode_length_content)
        if ti.decode_32bit_content:
            decode_32bit.append(ti.decode_32bit_content)
        if ti.decode_64bit_content:
            decode_64bit.append(ti.decode_64bit_content)
        if ti.dump_content:
            dump.append(ti.dump_content)

    cpp = ""
    if decode_varint:
        decode_varint.append("default:\n  return false;")
        o = f"bool {desc.name}::decode_varint(uint32_t field_id, ProtoVarInt value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_varint), "    ") + "\n"
        o += "  }\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_varint(uint32_t field_id, ProtoVarInt value) override;"
        protected_content.insert(0, prot)
    if decode_length:
        decode_length.append("default:\n  return false;")
        o = f"bool {desc.name}::decode_length(uint32_t field_id, ProtoLengthDelimited value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_length), "    ") + "\n"
        o += "  }\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;"
        protected_content.insert(0, prot)
    if decode_32bit:
        decode_32bit.append("default:\n  return false;")
        o = f"bool {desc.name}::decode_32bit(uint32_t field_id, Proto32Bit value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_32bit), "    ") + "\n"
        o += "  }\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_32bit(uint32_t field_id, Proto32Bit value) override;"
        protected_content.insert(0, prot)
    if decode_64bit:
        decode_64bit.append("default:\n  return false;")
        o = f"bool {desc.name}::decode_64bit(uint32_t field_id, Proto64Bit value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_64bit), "    ") + "\n"
        o += "  }\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_64bit(uint32_t field_id, Proto64Bit value) override;"
        protected_content.insert(0, prot)

    o = f"void {desc.name}::encode(ProtoWriteBuffer buffer) const {{"
    if encode:
        if len(encode) == 1 and len(encode[0]) + len(o) + 3 < 120:
            o += f" {encode[0]} "
        else:
            o += "\n"
            o += indent("\n".join(encode)) + "\n"
    o += "}\n"
    cpp += o
    prot = "void encode(ProtoWriteBuffer buffer) const override;"
    public_content.append(prot)

    # Add calculate_size method
    o = f"void {desc.name}::calculate_size(uint32_t &total_size) const {{"

    # Add a check for empty/default objects to short-circuit the calculation
    # Only add this optimization if we have fields to check
    if size_calc:
        # For a single field, just inline it for simplicity
        if len(size_calc) == 1 and len(size_calc[0]) + len(o) + 3 < 120:
            o += f" {size_calc[0]} "
        else:
            # For multiple fields, add a short-circuit check
            o += "\n"
            # Performance optimization: add all the size calculations
            o += indent("\n".join(size_calc)) + "\n"
    o += "}\n"
    cpp += o
    prot = "void calculate_size(uint32_t &total_size) const override;"
    public_content.append(prot)

    o = f"void {desc.name}::dump_to(std::string &out) const {{"
    if dump:
        if len(dump) == 1 and len(dump[0]) + len(o) + 3 < 120:
            o += f" {dump[0]} "
        else:
            o += "\n"
            o += "  __attribute__((unused)) char buffer[64];\n"
            o += f'  out.append("{desc.name} {{\\n");\n'
            o += indent("\n".join(dump)) + "\n"
            o += '  out.append("}");\n'
    else:
        o2 = f'out.append("{desc.name} {{}}");'
        if len(o) + len(o2) + 3 < 120:
            o += f" {o2} "
        else:
            o += "\n"
            o += f"  {o2}\n"
    o += "}\n"
    cpp += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    cpp += o
    cpp += "#endif\n"
    prot = "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    prot += "void dump_to(std::string &out) const override;\n"
    prot += "#endif\n"
    public_content.append(prot)

    out = f"class {desc.name} : public ProtoMessage {{\n"
    out += " public:\n"
    out += indent("\n".join(public_content)) + "\n"
    out += "\n"
    out += " protected:\n"
    out += indent("\n".join(protected_content))
    if len(protected_content) > 0:
        out += "\n"
    out += "};\n"
    return out, cpp


SOURCE_BOTH = 0
SOURCE_SERVER = 1
SOURCE_CLIENT = 2

RECEIVE_CASES: dict[int, str] = {}

ifdefs: dict[str, str] = {}


def get_opt(
    desc: descriptor.DescriptorProto,
    opt: descriptor.MessageOptions,
    default: Any = None,
) -> Any:
    """Get the option from the descriptor."""
    if not desc.options.HasExtension(opt):
        return default
    return desc.options.Extensions[opt]


def build_service_message_type(
    mt: descriptor.DescriptorProto,
) -> tuple[str, str] | None:
    """Builds the service message type."""
    snake = camel_to_snake(mt.name)
    id_: int | None = get_opt(mt, pb.id)
    if id_ is None:
        return None

    source: int = get_opt(mt, pb.source, 0)

    ifdef: str | None = get_opt(mt, pb.ifdef)
    log: bool = get_opt(mt, pb.log, True)
    hout = ""
    cout = ""

    if ifdef is not None:
        ifdefs[str(mt.name)] = ifdef
        hout += f"#ifdef {ifdef}\n"
        cout += f"#ifdef {ifdef}\n"

    if source in (SOURCE_BOTH, SOURCE_SERVER):
        # Generate send
        func = f"send_{snake}"
        hout += f"bool {func}(const {mt.name} &msg);\n"
        cout += f"bool APIServerConnectionBase::{func}(const {mt.name} &msg) {{\n"
        if log:
            cout += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
            cout += f'  ESP_LOGVV(TAG, "{func}: %s", msg.dump().c_str());\n'
            cout += "#endif\n"
        # cout += f'  this->set_nodelay({str(nodelay).lower()});\n'
        cout += f"  return this->send_message_<{mt.name}>(msg, {id_});\n"
        cout += "}\n"
    if source in (SOURCE_BOTH, SOURCE_CLIENT):
        # Generate receive
        func = f"on_{snake}"
        hout += f"virtual void {func}(const {mt.name} &value){{}};\n"
        case = ""
        if ifdef is not None:
            case += f"#ifdef {ifdef}\n"
        case += f"{mt.name} msg;\n"
        case += "msg.decode(msg_data, msg_size);\n"
        if log:
            case += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
            case += f'ESP_LOGVV(TAG, "{func}: %s", msg.dump().c_str());\n'
            case += "#endif\n"
        case += f"this->{func}(msg);\n"
        if ifdef is not None:
            case += "#endif\n"
        case += "break;"
        RECEIVE_CASES[id_] = case

    if ifdef is not None:
        hout += "#endif\n"
        cout += "#endif\n"

    return hout, cout


def main() -> None:
    """Main function to generate the C++ classes."""
    cwd = Path(__file__).resolve().parent
    root = cwd.parent.parent / "esphome" / "components" / "api"
    prot_file = root / "api.protoc"
    call(["protoc", "-o", str(prot_file), "-I", str(root), "api.proto"])
    proto_content = prot_file.read_bytes()

    # pylint: disable-next=no-member
    d = descriptor.FileDescriptorSet.FromString(proto_content)

    file = d.file[0]
    content = FILE_HEADER
    content += """\
    #pragma once

    #include "proto.h"
    #include "api_pb2_size.h"

    namespace esphome {
    namespace api {

    """

    cpp = FILE_HEADER
    cpp += """\
    #include "api_pb2.h"
    #include "api_pb2_size.h"
    #include "esphome/core/log.h"

    #include <cinttypes>

    namespace esphome {
    namespace api {

    """

    content += "namespace enums {\n\n"

    for enum in file.enum_type:
        s, c = build_enum_type(enum)
        content += s
        cpp += c

    content += "\n}  // namespace enums\n\n"

    mt = file.message_type

    for m in mt:
        s, c = build_message_type(m)
        content += s
        cpp += c

    content += """\

    }  // namespace api
    }  // namespace esphome
    """
    cpp += """\

    }  // namespace api
    }  // namespace esphome
    """

    with open(root / "api_pb2.h", "w", encoding="utf-8") as f:
        f.write(content)

    with open(root / "api_pb2.cpp", "w", encoding="utf-8") as f:
        f.write(cpp)

    hpp = FILE_HEADER
    hpp += """\
    #pragma once

    #include "api_pb2.h"
    #include "esphome/core/defines.h"

    namespace esphome {
    namespace api {

    """

    cpp = FILE_HEADER
    cpp += """\
    #include "api_pb2_service.h"
    #include "esphome/core/log.h"

    namespace esphome {
    namespace api {

    static const char *const TAG = "api.service";

    """

    class_name = "APIServerConnectionBase"

    hpp += f"class {class_name} : public ProtoService {{\n"
    hpp += " public:\n"

    for mt in file.message_type:
        obj = build_service_message_type(mt)
        if obj is None:
            continue
        hout, cout = obj
        hpp += indent(hout) + "\n"
        cpp += cout

    cases = list(RECEIVE_CASES.items())
    cases.sort()
    hpp += " protected:\n"
    hpp += "  bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;\n"
    out = f"bool {class_name}::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {{\n"
    out += "  switch (msg_type) {\n"
    for i, case in cases:
        c = f"case {i}: {{\n"
        c += indent(case) + "\n"
        c += "}"
        out += indent(c, "    ") + "\n"
    out += "    default:\n"
    out += "      return false;\n"
    out += "  }\n"
    out += "  return true;\n"
    out += "}\n"
    cpp += out
    hpp += "};\n"

    serv = file.service[0]
    class_name = "APIServerConnection"
    hpp += "\n"
    hpp += f"class {class_name} : public {class_name}Base {{\n"
    hpp += " public:\n"
    hpp_protected = ""
    cpp += "\n"

    m = serv.method[0]
    for m in serv.method:
        func = m.name
        inp = m.input_type[1:]
        ret = m.output_type[1:]
        is_void = ret == "void"
        snake = camel_to_snake(inp)
        on_func = f"on_{snake}"
        needs_conn = get_opt(m, pb.needs_setup_connection, True)
        needs_auth = get_opt(m, pb.needs_authentication, True)

        ifdef = ifdefs.get(inp, None)

        if ifdef is not None:
            hpp += f"#ifdef {ifdef}\n"
            hpp_protected += f"#ifdef {ifdef}\n"
            cpp += f"#ifdef {ifdef}\n"

        hpp_protected += f"  void {on_func}(const {inp} &msg) override;\n"
        hpp += f"  virtual {ret} {func}(const {inp} &msg) = 0;\n"
        cpp += f"void {class_name}::{on_func}(const {inp} &msg) {{\n"
        body = ""
        if needs_conn:
            body += "if (!this->is_connection_setup()) {\n"
            body += "  this->on_no_setup_connection();\n"
            body += "  return;\n"
            body += "}\n"
        if needs_auth:
            body += "if (!this->is_authenticated()) {\n"
            body += "  this->on_unauthenticated_access();\n"
            body += "  return;\n"
            body += "}\n"

        if is_void:
            body += f"this->{func}(msg);\n"
        else:
            body += f"{ret} ret = this->{func}(msg);\n"
            ret_snake = camel_to_snake(ret)
            body += f"if (!this->send_{ret_snake}(ret)) {{\n"
            body += "  this->on_fatal_error();\n"
            body += "}\n"
        cpp += indent(body) + "\n" + "}\n"

        if ifdef is not None:
            hpp += "#endif\n"
            hpp_protected += "#endif\n"
            cpp += "#endif\n"

    hpp += " protected:\n"
    hpp += hpp_protected
    hpp += "};\n"

    hpp += """\

    }  // namespace api
    }  // namespace esphome
    """
    cpp += """\

    }  // namespace api
    }  // namespace esphome
    """

    with open(root / "api_pb2_service.h", "w", encoding="utf-8") as f:
        f.write(hpp)

    with open(root / "api_pb2_service.cpp", "w", encoding="utf-8") as f:
        f.write(cpp)

    prot_file.unlink()

    try:
        import clang_format

        def exec_clang_format(path: Path) -> None:
            clang_format_path = os.path.join(
                os.path.dirname(clang_format.__file__), "data", "bin", "clang-format"
            )
            call([clang_format_path, "-i", path])

        exec_clang_format(root / "api_pb2_service.h")
        exec_clang_format(root / "api_pb2_service.cpp")
        exec_clang_format(root / "api_pb2.h")
        exec_clang_format(root / "api_pb2.cpp")
    except ImportError:
        pass


if __name__ == "__main__":
    sys.exit(main())
