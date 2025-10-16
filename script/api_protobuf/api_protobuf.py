#!/usr/bin/env python3
from __future__ import annotations

from abc import ABC, abstractmethod
from enum import IntEnum
from pathlib import Path
import re
from subprocess import call
import sys
from typing import Any

import aioesphomeapi.api_options_pb2 as pb
import google.protobuf.descriptor_pb2 as descriptor
from google.protobuf.descriptor_pb2 import FieldDescriptorProto


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
        if line == "" or line.startswith("#ifdef") or line.startswith("#endif"):
            p = ""
        else:
            p = padding
        lines.append(p + line)
    return lines


def indent(text: str, padding: str = "  ") -> str:
    return "\n".join(indent_list(text, padding))


def wrap_with_ifdef(content: str | list[str], ifdef: str | None) -> list[str]:
    """Wrap content with #ifdef directives if ifdef is provided.

    Args:
        content: Single string or list of strings to wrap
        ifdef: The ifdef condition, or None to skip wrapping

    Returns:
        List of strings with ifdef wrapping if needed
    """
    if not ifdef:
        if isinstance(content, str):
            return [content]
        return content

    result = [f"#ifdef {ifdef}"]
    if isinstance(content, str):
        result.append(content)
    else:
        result.extend(content)
    result.append("#endif")
    return result


def camel_to_snake(name: str) -> str:
    # https://stackoverflow.com/a/1176023
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s1).lower()


def force_str(force: bool) -> str:
    """Convert a boolean force value to string format for C++ code."""
    return str(force).lower()


class TypeInfo(ABC):
    """Base class for all type information."""

    def __init__(
        self,
        field: descriptor.FieldDescriptorProto,
        needs_decode: bool = True,
        needs_encode: bool = True,
    ) -> None:
        self._field = field
        self._needs_decode = needs_decode
        self._needs_encode = needs_encode

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
        return self._field.label == FieldDescriptorProto.LABEL_REPEATED

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
        return f"case {self.number}: this->{self.field_name} = {content}; break;"

    decode_varint = None

    @property
    def decode_length_content(self) -> str:
        content = self.decode_length
        if content is None:
            return None
        return f"case {self.number}: this->{self.field_name} = {content}; break;"

    decode_length = None

    @property
    def decode_32bit_content(self) -> str:
        content = self.decode_32bit
        if content is None:
            return None
        return f"case {self.number}: this->{self.field_name} = {content}; break;"

    decode_32bit = None

    @property
    def decode_64bit_content(self) -> str:
        content = self.decode_64bit
        if content is None:
            return None
        return f"case {self.number}: this->{self.field_name} = {content}; break;"

    decode_64bit = None

    @property
    def encode_content(self) -> str:
        return f"buffer.{self.encode_func}({self.number}, this->{self.field_name});"

    encode_func = None

    @classmethod
    def can_use_dump_field(cls) -> bool:
        """Whether this type can use the dump_field helper functions.

        Returns True for simple types that have dump_field overloads.
        Complex types like messages and bytes should return False.
        """
        return True

    def dump_field_value(self, value: str) -> str:
        """Get the value expression to pass to dump_field.

        Most types just pass the value directly, but some (like enums) need a cast.
        """
        return value

    @property
    def dump_content(self) -> str:
        # Default implementation - subclasses can override if they need special handling
        return f'dump_field(out, "{self.name}", {self.dump_field_value(f"this->{self.field_name}")});'

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

    def _get_simple_size_calculation(
        self, name: str, force: bool, base_method: str, value_expr: str = None
    ) -> str:
        """Helper for simple size calculations.

        Args:
            name: Field name
            force: Whether this is for a repeated field
            base_method: Base method name (e.g., "add_int32")
            value_expr: Optional value expression (defaults to name)
        """
        field_id_size = self.calculate_field_id_size()
        method = f"{base_method}_force" if force else base_method
        value = value_expr if value_expr else name
        return f"size.{method}({field_id_size}, {value});"

    @abstractmethod
    def get_size_calculation(self, name: str, force: bool = False) -> str:
        """Calculate the size needed for encoding this field.

        Args:
            name: The name of the field
            force: Whether to force encoding the field even if it has a default value
        """

    def get_fixed_size_bytes(self) -> int | None:
        """Get the number of bytes for fixed-size fields (float, double, fixed32, etc).

        Returns:
            The number of bytes (4 or 8) for fixed-size fields, None for variable-size fields.
        """
        return None

    @abstractmethod
    def get_estimated_size(self) -> int:
        """Get estimated size in bytes for this field with typical values.

        Returns:
            Estimated size in bytes including field ID and typical data
        """


TYPE_INFO: dict[int, TypeInfo] = {}

# Unsupported 64-bit types that would add overhead for embedded systems
# TYPE_DOUBLE = 1, TYPE_FIXED64 = 6, TYPE_SFIXED64 = 16, TYPE_SINT64 = 18
UNSUPPORTED_TYPES = {1: "double", 6: "fixed64", 16: "sfixed64", 18: "sint64"}


def validate_field_type(field_type: int, field_name: str = "") -> None:
    """Validate that the field type is supported by ESPHome API.

    Raises ValueError for unsupported 64-bit types.
    """
    if field_type in UNSUPPORTED_TYPES:
        type_name = UNSUPPORTED_TYPES[field_type]
        field_info = f" (field: {field_name})" if field_name else ""
        raise ValueError(
            f"64-bit type '{type_name}'{field_info} is not supported by ESPHome API. "
            "These types add significant overhead for embedded systems. "
            "If you need 64-bit support, please add the necessary encoding/decoding "
            "functions to proto.h/proto.cpp first."
        )


def create_field_type_info(
    field: descriptor.FieldDescriptorProto,
    needs_decode: bool = True,
    needs_encode: bool = True,
) -> TypeInfo:
    """Create the appropriate TypeInfo instance for a field, handling repeated fields and custom options."""
    if field.label == FieldDescriptorProto.LABEL_REPEATED:
        # Check if this repeated field has fixed_array_with_length_define option
        if (
            fixed_size := get_field_opt(field, pb.fixed_array_with_length_define)
        ) is not None:
            return FixedArrayWithLengthRepeatedType(field, fixed_size)
        # Check if this repeated field has fixed_array_size option
        if (fixed_size := get_field_opt(field, pb.fixed_array_size)) is not None:
            return FixedArrayRepeatedType(field, fixed_size)
        # Check if this repeated field has fixed_array_size_define option
        if (
            size_define := get_field_opt(field, pb.fixed_array_size_define)
        ) is not None:
            return FixedArrayRepeatedType(field, size_define)
        return RepeatedTypeInfo(field)

    # Check for mutually exclusive options on bytes fields
    if field.type == 12:
        has_pointer_to_buffer = get_field_opt(field, pb.pointer_to_buffer, False)
        fixed_size = get_field_opt(field, pb.fixed_array_size, None)

        if has_pointer_to_buffer and fixed_size is not None:
            raise ValueError(
                f"Field '{field.name}' has both pointer_to_buffer and fixed_array_size. "
                "These options are mutually exclusive. Use pointer_to_buffer for zero-copy "
                "or fixed_array_size for traditional array storage."
            )

        if has_pointer_to_buffer:
            # Zero-copy pointer approach - no size needed, will use size_t for length
            return PointerToBytesBufferType(field, None)

        if fixed_size is not None:
            # Traditional fixed array approach with copy
            return FixedArrayBytesType(field, fixed_size)

    # Check for pointer_to_buffer option on string fields
    if field.type == 9:
        has_pointer_to_buffer = get_field_opt(field, pb.pointer_to_buffer, False)

        if has_pointer_to_buffer:
            # Zero-copy pointer approach for strings
            return PointerToBytesBufferType(field, None)

    # Special handling for bytes fields
    if field.type == 12:
        return BytesType(field, needs_decode, needs_encode)

    # Special handling for string fields
    if field.type == 9:
        return StringType(field, needs_decode, needs_encode)

    validate_field_type(field.type, field.name)
    return TYPE_INFO[field.type](field)


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
        o = f'snprintf(buffer, sizeof(buffer), "%g", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_double({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 8

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 8  # field ID + 8 bytes for double


@register_type(2)
class FloatType(TypeInfo):
    cpp_type = "float"
    default_value = "0.0f"
    decode_32bit = "value.as_float()"
    encode_func = "encode_float"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%g", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_float({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 4

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 4  # field ID + 4 bytes for float


@register_type(3)
class Int64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_int64()"
    encode_func = "encode_int64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_int64")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


@register_type(4)
class UInt64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_varint = "value.as_uint64()"
    encode_func = "encode_uint64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%llu", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_uint64")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


@register_type(5)
class Int32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_int32()"
    encode_func = "encode_int32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_int32")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


@register_type(6)
class Fixed64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_64bit = "value.as_fixed64()"
    encode_func = "encode_fixed64"
    wire_type = WireType.FIXED64  # Uses wire type 1

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%llu", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_fixed64({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 8

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 8  # field ID + 8 bytes fixed


@register_type(7)
class Fixed32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_32bit = "value.as_fixed32()"
    encode_func = "encode_fixed32"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%" PRIu32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_fixed32({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 4

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 4  # field ID + 4 bytes fixed


@register_type(8)
class BoolType(TypeInfo):
    cpp_type = "bool"
    default_value = "false"
    decode_varint = "value.as_bool()"
    encode_func = "encode_bool"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        return f"out.append(YESNO({name}));"

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_bool")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 1  # field ID + 1 byte


@register_type(9)
class StringType(TypeInfo):
    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    decode_length = "value.as_string()"
    encode_func = "encode_string"
    wire_type = WireType.LENGTH_DELIMITED  # Uses wire type 2

    @property
    def public_content(self) -> list[str]:
        content: list[str] = []

        # Check if no_zero_copy option is set
        no_zero_copy = get_field_opt(self._field, pb.no_zero_copy, False)

        # Add std::string storage if message needs decoding OR if no_zero_copy is set
        if self._needs_decode or no_zero_copy:
            content.append(f"std::string {self.field_name}{{}};")

        # Only add StringRef if encoding is needed AND no_zero_copy is not set
        if self._needs_encode and not no_zero_copy:
            content.extend(
                [
                    # Add StringRef field if message needs encoding
                    f"StringRef {self.field_name}_ref_{{}};",
                    # Add setter method if message needs encoding
                    f"void set_{self.field_name}(const StringRef &ref) {{",
                    f"  this->{self.field_name}_ref_ = ref;",
                    "}",
                ]
            )
        return content

    @property
    def encode_content(self) -> str:
        # Check if no_zero_copy option is set
        no_zero_copy = get_field_opt(self._field, pb.no_zero_copy, False)

        if no_zero_copy:
            # Use the std::string directly
            return f"buffer.encode_string({self.number}, this->{self.field_name});"
        # Use the StringRef
        return f"buffer.encode_string({self.number}, this->{self.field_name}_ref_);"

    def dump(self, name):
        # Check if no_zero_copy option is set
        no_zero_copy = get_field_opt(self._field, pb.no_zero_copy, False)

        # If name is 'it', this is a repeated field element - always use string
        if name == "it":
            return "append_quoted_string(out, StringRef(it));"

        # If no_zero_copy is set, always use std::string
        if no_zero_copy:
            return f'out.append("\'").append(this->{self.field_name}).append("\'");'

        # For SOURCE_CLIENT only, always use std::string
        if not self._needs_encode:
            return f'out.append("\'").append(this->{self.field_name}).append("\'");'

        # For SOURCE_SERVER, always use StringRef
        if not self._needs_decode:
            return f"append_quoted_string(out, this->{self.field_name}_ref_);"

        # For SOURCE_BOTH, check if StringRef is set (sending) or use string (received)
        return (
            f"if (!this->{self.field_name}_ref_.empty()) {{"
            f'  out.append("\'").append(this->{self.field_name}_ref_.c_str()).append("\'");'
            f"}} else {{"
            f'  out.append("\'").append(this->{self.field_name}).append("\'");'
            f"}}"
        )

    @property
    def dump_content(self) -> str:
        # Check if no_zero_copy option is set
        no_zero_copy = get_field_opt(self._field, pb.no_zero_copy, False)

        # If no_zero_copy is set, always use std::string
        if no_zero_copy:
            return f'dump_field(out, "{self.name}", this->{self.field_name});'

        # For SOURCE_CLIENT only, use std::string
        if not self._needs_encode:
            return f'dump_field(out, "{self.name}", this->{self.field_name});'

        # For SOURCE_SERVER, use StringRef with _ref_ suffix
        if not self._needs_decode:
            return f'dump_field(out, "{self.name}", this->{self.field_name}_ref_);'

        # For SOURCE_BOTH, we need custom logic
        o = f'out.append("  {self.name}: ");\n'
        o += self.dump(f"this->{self.field_name}") + "\n"
        o += 'out.append("\\n");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # Check if no_zero_copy option is set
        no_zero_copy = get_field_opt(self._field, pb.no_zero_copy, False)

        # For SOURCE_CLIENT only messages or no_zero_copy, use the string field directly
        if not self._needs_encode or no_zero_copy:
            # For no_zero_copy, we need to use .size() on the string
            if no_zero_copy and name != "it":
                field_id_size = self.calculate_field_id_size()
                return (
                    f"size.add_length({field_id_size}, this->{self.field_name}.size());"
                )
            return self._get_simple_size_calculation(name, force, "add_length")

        # Check if this is being called from a repeated field context
        # In that case, 'name' will be 'it' and we need to use the repeated version
        if name == "it":
            # For repeated fields, we need to use add_length_force which includes field ID
            field_id_size = self.calculate_field_id_size()
            return f"size.add_length_force({field_id_size}, it.size());"

        # For messages that need encoding, use the StringRef size
        field_id_size = self.calculate_field_id_size()
        return f"size.add_length({field_id_size}, this->{self.field_name}_ref_.size());"

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 8  # field ID + 8 bytes typical string


@register_type(11)
class MessageType(TypeInfo):
    @classmethod
    def can_use_dump_field(cls) -> bool:
        return False

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
        return "encode_message"

    @property
    def decode_length(self) -> str:
        # Override to return None for message types because we can't use template-based
        # decoding when the specific message type isn't known at compile time.
        # Instead, we use the non-template decode_to_message() method which allows
        # runtime polymorphism through virtual function calls.
        return None

    @property
    def decode_length_content(self) -> str:
        # Custom decode that doesn't use templates
        return f"case {self.number}: value.decode_to_message(this->{self.field_name}); break;"

    def dump(self, name: str) -> str:
        return f"{name}.dump_to(out);"

    @property
    def dump_content(self) -> str:
        o = f'out.append("  {self.name}: ");\n'
        o += f"this->{self.field_name}.dump_to(out);\n"
        o += 'out.append("\\n");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_message_object")

    def get_estimated_size(self) -> int:
        # For message types, we can't easily estimate the submessage size without
        # access to the actual message definition. This is just a rough estimate.
        return (
            self.calculate_field_id_size() + 16
        )  # field ID + 16 bytes estimated submessage


@register_type(12)
class BytesType(TypeInfo):
    @classmethod
    def can_use_dump_field(cls) -> bool:
        return False

    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    encode_func = "encode_bytes"
    decode_length = "value.as_string()"
    wire_type = WireType.LENGTH_DELIMITED  # Uses wire type 2

    @property
    def public_content(self) -> list[str]:
        content: list[str] = []
        # Add std::string storage if message needs decoding
        if self._needs_decode:
            content.append(f"std::string {self.field_name}{{}};")

        if self._needs_encode:
            content.extend(
                [
                    # Add pointer/length fields if message needs encoding
                    f"const uint8_t* {self.field_name}_ptr_{{nullptr}};",
                    f"size_t {self.field_name}_len_{{0}};",
                    # Add setter method if message needs encoding
                    f"void set_{self.field_name}(const uint8_t* data, size_t len) {{",
                    f"  this->{self.field_name}_ptr_ = data;",
                    f"  this->{self.field_name}_len_ = len;",
                    "}",
                ]
            )
        return content

    @property
    def encode_content(self) -> str:
        return f"buffer.encode_bytes({self.number}, this->{self.field_name}_ptr_, this->{self.field_name}_len_);"

    def dump(self, name: str) -> str:
        ptr_dump = f"format_hex_pretty(this->{self.field_name}_ptr_, this->{self.field_name}_len_)"
        str_dump = f"format_hex_pretty(reinterpret_cast<const uint8_t*>(this->{self.field_name}.data()), this->{self.field_name}.size())"

        # For SOURCE_CLIENT only, always use std::string
        if not self._needs_encode:
            return f"out.append({str_dump});"

        # For SOURCE_SERVER, always use pointer/length
        if not self._needs_decode:
            return f"out.append({ptr_dump});"

        # For SOURCE_BOTH, check if pointer is set (sending) or use string (received)
        return (
            f"if (this->{self.field_name}_ptr_ != nullptr) {{\n"
            f"    out.append({ptr_dump});\n"
            f"  }} else {{\n"
            f"    out.append({str_dump});\n"
            f"  }}"
        )

    @property
    def dump_content(self) -> str:
        o = f'out.append("  {self.name}: ");\n'
        o += self.dump(f"this->{self.field_name}") + "\n"
        o += 'out.append("\\n");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return f"size.add_length({self.calculate_field_id_size()}, this->{self.field_name}_len_);"

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 8  # field ID + 8 bytes typical bytes


class PointerToBytesBufferType(TypeInfo):
    """Type for bytes fields that use pointer_to_buffer option for zero-copy."""

    @classmethod
    def can_use_dump_field(cls) -> bool:
        return False

    def __init__(
        self, field: descriptor.FieldDescriptorProto, size: int | None = None
    ) -> None:
        super().__init__(field)
        # Size is not used for pointer_to_buffer - we always use size_t for length
        self.array_size = 0

    @property
    def cpp_type(self) -> str:
        return "const uint8_t*"

    @property
    def default_value(self) -> str:
        return "nullptr"

    @property
    def reference_type(self) -> str:
        return "const uint8_t*"

    @property
    def const_reference_type(self) -> str:
        return "const uint8_t*"

    @property
    def public_content(self) -> list[str]:
        # Use uint16_t for length - max packet size is well below 65535
        # Add pointer and length fields
        return [
            f"const uint8_t* {self.field_name}{{nullptr}};",
            f"uint16_t {self.field_name}_len{{0}};",
        ]

    @property
    def encode_content(self) -> str:
        return f"buffer.encode_bytes({self.number}, this->{self.field_name}, this->{self.field_name}_len);"

    @property
    def decode_length_content(self) -> str | None:
        # Decode directly stores the pointer to avoid allocation
        return f"""case {self.number}: {{
      // Use raw data directly to avoid allocation
      this->{self.field_name} = value.data();
      this->{self.field_name}_len = value.size();
      break;
    }}"""

    @property
    def decode_length(self) -> str | None:
        # This is handled in decode_length_content
        return None

    @property
    def wire_type(self) -> WireType:
        """Get the wire type for this bytes field."""
        return WireType.LENGTH_DELIMITED  # Uses wire type 2

    def dump(self, name: str) -> str:
        return (
            f"format_hex_pretty(this->{self.field_name}, this->{self.field_name}_len)"
        )

    @property
    def dump_content(self) -> str:
        # Custom dump that doesn't use dump_field template
        return (
            f'out.append("  {self.name}: ");\n'
            + f"out.append({self.dump(self.field_name)});\n"
            + 'out.append("\\n");'
        )

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return f"size.add_length({self.number}, this->{self.field_name}_len);"

    def get_estimated_size(self) -> int:
        # field ID + length varint + typical data (assume small for pointer fields)
        return self.calculate_field_id_size() + 2 + 16


class FixedArrayBytesType(TypeInfo):
    """Special type for fixed-size byte arrays."""

    @classmethod
    def can_use_dump_field(cls) -> bool:
        return False

    def __init__(self, field: descriptor.FieldDescriptorProto, size: int) -> None:
        super().__init__(field)
        self.array_size = size

    @property
    def cpp_type(self) -> str:
        return "uint8_t"

    @property
    def default_value(self) -> str:
        return "{}"

    @property
    def reference_type(self) -> str:
        return f"uint8_t (&)[{self.array_size}]"

    @property
    def const_reference_type(self) -> str:
        return f"const uint8_t (&)[{self.array_size}]"

    @property
    def public_content(self) -> list[str]:
        len_type = (
            "uint8_t"
            if self.array_size <= 255
            else "uint16_t"
            if self.array_size <= 65535
            else "size_t"
        )
        # Add both the array and length fields
        return [
            f"uint8_t {self.field_name}[{self.array_size}]{{}};",
            f"{len_type} {self.field_name}_len{{0}};",
        ]

    @property
    def decode_length_content(self) -> str:
        o = f"case {self.number}: {{\n"
        o += "  const std::string &data_str = value.as_string();\n"
        o += f"  this->{self.field_name}_len = data_str.size();\n"
        o += f"  if (this->{self.field_name}_len > {self.array_size}) {{\n"
        o += f"    this->{self.field_name}_len = {self.array_size};\n"
        o += "  }\n"
        o += f"  memcpy(this->{self.field_name}, data_str.data(), this->{self.field_name}_len);\n"
        o += "  break;\n"
        o += "}"
        return o

    @property
    def encode_content(self) -> str:
        return f"buffer.encode_bytes({self.number}, this->{self.field_name}, this->{self.field_name}_len);"

    def dump(self, name: str) -> str:
        return f"out.append(format_hex_pretty({name}, {name}_len));"

    @property
    def dump_content(self) -> str:
        o = f'out.append("  {self.name}: ");\n'
        o += f"out.append(format_hex_pretty(this->{self.field_name}, this->{self.field_name}_len));\n"
        o += 'out.append("\\n");'
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # Use the actual length stored in the _len field
        length_field = f"this->{self.field_name}_len"
        field_id_size = self.calculate_field_id_size()

        if force:
            # For repeated fields, always calculate size (no zero check)
            return f"size.add_length_force({field_id_size}, {length_field});"
        # For non-repeated fields, add_length already checks for zero
        return f"size.add_length({field_id_size}, {length_field});"

    def get_estimated_size(self) -> int:
        # Estimate based on typical BLE advertisement size
        return (
            self.calculate_field_id_size() + 1 + 31
        )  # field ID + length byte + typical 31 bytes

    @property
    def wire_type(self) -> WireType:
        return WireType.LENGTH_DELIMITED


@register_type(13)
class UInt32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_varint = "value.as_uint32()"
    encode_func = "encode_uint32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%" PRIu32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_uint32")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


@register_type(14)
class EnumType(TypeInfo):
    @property
    def cpp_type(self) -> str:
        return f"enums::{self._field.type_name[1:]}"

    @property
    def decode_varint(self) -> str:
        return f"static_cast<{self.cpp_type}>(value.as_uint32())"

    default_value = ""
    wire_type = WireType.VARINT  # Uses wire type 0

    @property
    def encode_func(self) -> str:
        return "encode_uint32"

    @property
    def encode_content(self) -> str:
        return f"buffer.{self.encode_func}({self.number}, static_cast<uint32_t>(this->{self.field_name}));"

    def dump(self, name: str) -> str:
        return f"out.append(proto_enum_to_string<{self.cpp_type}>({name}));"

    def dump_field_value(self, value: str) -> str:
        # Enums need explicit cast for the template
        return f"static_cast<{self.cpp_type}>({value})"

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(
            name, force, "add_uint32", f"static_cast<uint32_t>({name})"
        )

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 1  # field ID + 1 byte typical enum


@register_type(15)
class SFixed32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_32bit = "value.as_sfixed32()"
    encode_func = "encode_sfixed32"
    wire_type = WireType.FIXED32  # Uses wire type 5

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_sfixed32({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 4

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 4  # field ID + 4 bytes fixed


@register_type(16)
class SFixed64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_64bit = "value.as_sfixed64()"
    encode_func = "encode_sfixed64"
    wire_type = WireType.FIXED64  # Uses wire type 1

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        field_id_size = self.calculate_field_id_size()
        return f"size.add_sfixed64({field_id_size}, {name});"

    def get_fixed_size_bytes(self) -> int:
        return 8

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 8  # field ID + 8 bytes fixed


@register_type(17)
class SInt32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_sint32()"
    encode_func = "encode_sint32"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%" PRId32, {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_sint32")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


@register_type(18)
class SInt64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_sint64()"
    encode_func = "encode_sint64"
    wire_type = WireType.VARINT  # Uses wire type 0

    def dump(self, name: str) -> str:
        o = f'snprintf(buffer, sizeof(buffer), "%lld", {name});\n'
        o += "out.append(buffer);"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        return self._get_simple_size_calculation(name, force, "add_sint64")

    def get_estimated_size(self) -> int:
        return self.calculate_field_id_size() + 3  # field ID + 3 bytes typical varint


def _generate_array_dump_content(
    ti, field_name: str, name: str, is_bool: bool = False
) -> str:
    """Generate dump content for array types (repeated or fixed array).

    Shared helper to avoid code duplication between RepeatedTypeInfo and FixedArrayRepeatedType.
    """
    o = f"for (const auto {'' if is_bool else '&'}it : {field_name}) {{\n"
    # Check if underlying type can use dump_field
    if ti.can_use_dump_field():
        # For types that have dump_field overloads, use them with extra indent
        # std::vector<bool> iterators return proxy objects, need explicit cast
        value_expr = "static_cast<bool>(it)" if is_bool else ti.dump_field_value("it")
        o += f'  dump_field(out, "{name}", {value_expr}, 4);\n'
    else:
        # For complex types (messages, bytes), use the old pattern
        o += f'  out.append("  {name}: ");\n'
        o += indent(ti.dump("it")) + "\n"
        o += '  out.append("\\n");\n'
    o += "}"
    return o


class FixedArrayRepeatedType(TypeInfo):
    """Special type for fixed-size repeated fields using std::array.

    Fixed arrays are only supported for encoding (SOURCE_SERVER) since we cannot
    control how many items we receive when decoding.
    """

    def __init__(self, field: descriptor.FieldDescriptorProto, size: int | str) -> None:
        super().__init__(field)
        self.array_size = size
        self.is_define = isinstance(size, str)
        # Check if we should skip encoding when all elements are zero
        # Use getattr to handle older versions of api_options_pb2
        self.skip_zero = get_field_opt(
            field, getattr(pb, "fixed_array_skip_zero", None), False
        )
        # Create the element type info
        validate_field_type(field.type, field.name)
        self._ti: TypeInfo = TYPE_INFO[field.type](field)

    def _encode_element(self, element: str) -> str:
        """Helper to generate encode statement for a single element."""
        if isinstance(self._ti, EnumType):
            return f"buffer.{self._ti.encode_func}({self.number}, static_cast<uint32_t>({element}), true);"
        return f"buffer.{self._ti.encode_func}({self.number}, {element}, true);"

    @property
    def cpp_type(self) -> str:
        return f"std::array<{self._ti.cpp_type}, {self.array_size}>"

    @property
    def reference_type(self) -> str:
        return f"{self.cpp_type} &"

    @property
    def const_reference_type(self) -> str:
        return f"const {self.cpp_type} &"

    @property
    def wire_type(self) -> WireType:
        """Get the wire type for this fixed array field."""
        return self._ti.wire_type

    @property
    def public_content(self) -> list[str]:
        # Just the array member, no index needed since we don't decode
        return [f"{self.cpp_type} {self.field_name}{{}};"]

    # No decode methods needed - fixed arrays don't support decoding
    # The base class TypeInfo already returns None for all decode properties

    @property
    def encode_content(self) -> str:
        # If skip_zero is enabled, wrap encoding in a zero check
        if self.skip_zero:
            if self.is_define:
                # When using a define, we need to use a loop-based approach
                o = f"for (const auto &it : this->{self.field_name}) {{\n"
                o += "  if (it != 0) {\n"
                o += f"    {self._encode_element('it')}\n"
                o += "  }\n"
                o += "}"
                return o
            # Build the condition to check if at least one element is non-zero
            non_zero_checks = " || ".join(
                [f"this->{self.field_name}[{i}] != 0" for i in range(self.array_size)]
            )
            encode_lines = [
                f"  {self._encode_element(f'this->{self.field_name}[{i}]')}"
                for i in range(self.array_size)
            ]
            return f"if ({non_zero_checks}) {{\n" + "\n".join(encode_lines) + "\n}"

        # When using a define, always use loop-based approach
        if self.is_define:
            o = f"for (const auto &it : this->{self.field_name}) {{\n"
            o += f"  {self._encode_element('it')}\n"
            o += "}"
            return o

        # Unroll small arrays for efficiency
        if self.array_size == 1:
            return self._encode_element(f"this->{self.field_name}[0]")
        if self.array_size == 2:
            return (
                self._encode_element(f"this->{self.field_name}[0]")
                + "\n  "
                + self._encode_element(f"this->{self.field_name}[1]")
            )

        # Use loops for larger arrays
        o = f"for (const auto &it : this->{self.field_name}) {{\n"
        o += f"  {self._encode_element('it')}\n"
        o += "}"
        return o

    @property
    def dump_content(self) -> str:
        return _generate_array_dump_content(
            self._ti, f"this->{self.field_name}", self.name, is_bool=False
        )

    def dump(self, name: str) -> str:
        # This is used when dumping the array itself (not its elements)
        # Since dump_content handles the iteration, this is not used directly
        return ""

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # If skip_zero is enabled, wrap size calculation in a zero check
        if self.skip_zero:
            if self.is_define:
                # When using a define, we need to use a loop-based approach
                o = f"for (const auto &it : {name}) {{\n"
                o += "  if (it != 0) {\n"
                o += f"    {self._ti.get_size_calculation('it', True)}\n"
                o += "  }\n"
                o += "}"
                return o
            # Build the condition to check if at least one element is non-zero
            non_zero_checks = " || ".join(
                [f"{name}[{i}] != 0" for i in range(self.array_size)]
            )
            size_lines = [
                f"  {self._ti.get_size_calculation(f'{name}[{i}]', True)}"
                for i in range(self.array_size)
            ]
            return f"if ({non_zero_checks}) {{\n" + "\n".join(size_lines) + "\n}"

        # When using a define, always use loop-based approach
        if self.is_define:
            o = f"for (const auto &it : {name}) {{\n"
            o += f"  {self._ti.get_size_calculation('it', True)}\n"
            o += "}"
            return o

        # For fixed arrays, we always encode all elements

        # Special case for single-element arrays - no loop needed
        if self.array_size == 1:
            return self._ti.get_size_calculation(f"{name}[0]", True)

        # Special case for 2-element arrays - unroll the calculation
        if self.array_size == 2:
            return (
                self._ti.get_size_calculation(f"{name}[0]", True)
                + "\n  "
                + self._ti.get_size_calculation(f"{name}[1]", True)
            )

        # Use loops for larger arrays
        o = f"for (const auto &it : {name}) {{\n"
        o += f"  {self._ti.get_size_calculation('it', True)}\n"
        o += "}"
        return o

    def get_estimated_size(self) -> int:
        # For fixed arrays, estimate underlying type size * array size
        underlying_size = self._ti.get_estimated_size()
        if self.is_define:
            # When using a define, we don't know the actual size so just guess 3
            # This is only used for documentation and never actually used since
            # fixed arrays are only for SOURCE_SERVER (encode-only) messages
            return underlying_size * 3
        return underlying_size * self.array_size


class FixedArrayWithLengthRepeatedType(FixedArrayRepeatedType):
    """Special type for fixed-size repeated fields with variable length tracking.

    Similar to FixedArrayRepeatedType but generates an additional length field
    to track how many elements are actually in use. Only encodes/sends elements
    up to the current length.

    Fixed arrays with length are only supported for encoding (SOURCE_SERVER) since
    we cannot control how many items we receive when decoding.
    """

    @property
    def public_content(self) -> list[str]:
        # Return both the array and the length field
        return [
            f"{self.cpp_type} {self.field_name}{{}};",
            f"uint16_t {self.field_name}_len{{0}};",
        ]

    @property
    def encode_content(self) -> str:
        # Always use a loop up to the current length
        o = f"for (uint16_t i = 0; i < this->{self.field_name}_len; i++) {{\n"
        o += f"  {self._encode_element(f'this->{self.field_name}[i]')}\n"
        o += "}"
        return o

    @property
    def dump_content(self) -> str:
        # Dump only the active elements
        o = f"for (uint16_t i = 0; i < this->{self.field_name}_len; i++) {{\n"
        # Check if underlying type can use dump_field
        if self._ti.can_use_dump_field():
            o += f'  dump_field(out, "{self.name}", {self._ti.dump_field_value(f"this->{self.field_name}[i]")}, 4);\n'
        else:
            o += f'  out.append("  {self.name}: ");\n'
            o += indent(self._ti.dump(f"this->{self.field_name}[i]")) + "\n"
            o += '  out.append("\\n");\n'
        o += "}"
        return o

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # Calculate size only for active elements
        o = f"for (uint16_t i = 0; i < {name}_len; i++) {{\n"
        o += f"  {self._ti.get_size_calculation(f'{name}[i]', True)}\n"
        o += "}"
        return o

    def get_estimated_size(self) -> int:
        # For fixed arrays with length, estimate based on typical usage
        # Assume on average half the array is used
        underlying_size = self._ti.get_estimated_size()
        if self.is_define:
            # When using a define, estimate 8 elements as typical
            return underlying_size * 8
        return underlying_size * (
            self.array_size // 2 if self.array_size > 2 else self.array_size
        )


class RepeatedTypeInfo(TypeInfo):
    def __init__(self, field: descriptor.FieldDescriptorProto) -> None:
        super().__init__(field)
        # Check if this is a pointer field by looking for container_pointer option
        self._container_type = get_field_opt(field, pb.container_pointer, "")
        self._use_pointer = bool(self._container_type)
        # Check if this should use FixedVector instead of std::vector
        self._use_fixed_vector = get_field_opt(field, pb.fixed_vector, False)

        # For repeated fields, we need to get the base type info
        # but we can't call create_field_type_info as it would cause recursion
        # So we extract just the type creation logic
        if (
            field.type == 12
            and (fixed_size := get_field_opt(field, pb.fixed_array_size)) is not None
        ):
            self._ti: TypeInfo = FixedArrayBytesType(field, fixed_size)
            return

        validate_field_type(field.type, field.name)
        self._ti: TypeInfo = TYPE_INFO[field.type](field)

    @property
    def cpp_type(self) -> str:
        if self._use_pointer and self._container_type:
            # For pointer fields, use the specified container type
            # If the container type already includes the element type (e.g., std::set<climate::ClimateMode>)
            # use it as-is, otherwise append the element type
            if "<" in self._container_type and ">" in self._container_type:
                return f"const {self._container_type}*"
            return f"const {self._container_type}<{self._ti.cpp_type}>*"
        if self._use_fixed_vector:
            return f"FixedVector<{self._ti.cpp_type}>"
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
        # Pointer fields don't support decoding
        if self._use_pointer:
            return None
        content = self._ti.decode_varint
        if content is None:
            return None
        return (
            f"case {self.number}: this->{self.field_name}.push_back({content}); break;"
        )

    @property
    def decode_length_content(self) -> str:
        # Pointer fields don't support decoding
        if self._use_pointer:
            return None
        content = self._ti.decode_length
        if content is None and isinstance(self._ti, MessageType):
            # Special handling for non-template message decoding
            return f"case {self.number}: this->{self.field_name}.emplace_back(); value.decode_to_message(this->{self.field_name}.back()); break;"
        if content is None:
            return None
        return (
            f"case {self.number}: this->{self.field_name}.push_back({content}); break;"
        )

    @property
    def decode_32bit_content(self) -> str:
        # Pointer fields don't support decoding
        if self._use_pointer:
            return None
        content = self._ti.decode_32bit
        if content is None:
            return None
        return (
            f"case {self.number}: this->{self.field_name}.push_back({content}); break;"
        )

    @property
    def decode_64bit_content(self) -> str:
        # Pointer fields don't support decoding
        if self._use_pointer:
            return None
        content = self._ti.decode_64bit
        if content is None:
            return None
        return (
            f"case {self.number}: this->{self.field_name}.push_back({content}); break;"
        )

    @property
    def _ti_is_bool(self) -> bool:
        # std::vector is specialized for bool, reference does not work
        return isinstance(self._ti, BoolType)

    @property
    def encode_content(self) -> str:
        if self._use_pointer:
            # For pointer fields, just dereference (pointer should never be null in our use case)
            o = f"for (const auto &it : *this->{self.field_name}) {{\n"
            if isinstance(self._ti, EnumType):
                o += f"  buffer.{self._ti.encode_func}({self.number}, static_cast<uint32_t>(it), true);\n"
            else:
                o += f"  buffer.{self._ti.encode_func}({self.number}, it, true);\n"
            o += "}"
            return o
        o = f"for (auto {'' if self._ti_is_bool else '&'}it : this->{self.field_name}) {{\n"
        if isinstance(self._ti, EnumType):
            o += f"  buffer.{self._ti.encode_func}({self.number}, static_cast<uint32_t>(it), true);\n"
        else:
            o += f"  buffer.{self._ti.encode_func}({self.number}, it, true);\n"
        o += "}"
        return o

    @property
    def dump_content(self) -> str:
        if self._use_pointer:
            # For pointer fields, dereference and use the existing helper
            return _generate_array_dump_content(
                self._ti, f"*this->{self.field_name}", self.name, is_bool=False
            )
        return _generate_array_dump_content(
            self._ti, f"this->{self.field_name}", self.name, is_bool=self._ti_is_bool
        )

    def dump(self, _: str):
        pass

    def get_size_calculation(self, name: str, force: bool = False) -> str:
        # For repeated fields, we always need to pass force=True to the underlying type's calculation
        # This is because the encode method always sets force=true for repeated fields

        # Handle message types separately as they use a dedicated helper
        if isinstance(self._ti, MessageType):
            field_id_size = self._ti.calculate_field_id_size()
            container = f"*{name}" if self._use_pointer else name
            return f"size.add_repeated_message({field_id_size}, {container});"

        # For non-message types, generate size calculation with iteration
        container_ref = f"*{name}" if self._use_pointer else name
        empty_check = f"{name}->empty()" if self._use_pointer else f"{name}.empty()"

        o = f"if (!{empty_check}) {{\n"

        # Check if this is a fixed-size type
        num_bytes = self._ti.get_fixed_size_bytes()
        if num_bytes is not None:
            # Fixed types have constant size per element
            field_id_size = self._ti.calculate_field_id_size()
            bytes_per_element = field_id_size + num_bytes
            size_expr = f"{name}->size()" if self._use_pointer else f"{name}.size()"
            o += f"  size.add_precalculated_size({size_expr} * {bytes_per_element});\n"
        else:
            # Other types need the actual value
            auto_ref = "" if self._ti_is_bool else "&"
            o += f"  for (const auto {auto_ref}it : {container_ref}) {{\n"
            o += f"    {self._ti.get_size_calculation('it', True)}\n"
            o += "  }\n"

        o += "}"
        return o

    def get_estimated_size(self) -> int:
        # For repeated fields, estimate underlying type size * 2 (assume 2 items typically)
        underlying_size = (
            self._ti.get_estimated_size()
            if hasattr(self._ti, "get_estimated_size")
            else 8
        )
        return underlying_size * 2


def build_type_usage_map(
    file_desc: descriptor.FileDescriptorProto,
) -> tuple[dict[str, str | None], dict[str, str | None], dict[str, int], set[str]]:
    """Build mappings for both enums and messages to their ifdefs based on usage.

    Returns:
        tuple: (enum_ifdef_map, message_ifdef_map, message_source_map, used_messages)
    """
    enum_ifdef_map: dict[str, str | None] = {}
    message_ifdef_map: dict[str, str | None] = {}
    message_source_map: dict[str, int] = {}

    # Build maps of which types are used by which messages
    enum_usage: dict[
        str, set[str]
    ] = {}  # enum_name -> set of message names that use it
    message_usage: dict[
        str, set[str]
    ] = {}  # message_name -> set of message names that use it
    used_messages: set[str] = set()  # Track which messages are actually used

    # Build message name to ifdef mapping for quick lookup
    message_to_ifdef: dict[str, str | None] = {
        msg.name: get_opt(msg, pb.ifdef) for msg in file_desc.message_type
    }

    # Analyze field usage
    # Also track field_ifdef for message types
    message_field_ifdefs: dict[
        str, set[str | None]
    ] = {}  # message_name -> set of field_ifdefs that use it

    for message in file_desc.message_type:
        # Skip deprecated messages entirely
        if message.options.deprecated:
            continue

        for field in message.field:
            # Skip deprecated fields when tracking enum usage
            if field.options.deprecated:
                continue

            type_name = field.type_name.split(".")[-1] if field.type_name else None
            if not type_name:
                continue

            # Track enum usage (only from non-deprecated fields)
            if field.type == 14:  # TYPE_ENUM
                enum_usage.setdefault(type_name, set()).add(message.name)
            # Track message usage
            elif field.type == 11:  # TYPE_MESSAGE
                message_usage.setdefault(type_name, set()).add(message.name)
                # Also track the field_ifdef if present
                field_ifdef = get_field_opt(field, pb.field_ifdef)
                message_field_ifdefs.setdefault(type_name, set()).add(field_ifdef)
                used_messages.add(type_name)

    # Helper to get unique ifdef from a set of messages
    def get_unique_ifdef(message_names: set[str]) -> str | None:
        ifdefs: set[str] = {
            message_to_ifdef[name]
            for name in message_names
            if message_to_ifdef.get(name)
        }
        return ifdefs.pop() if len(ifdefs) == 1 else None

    # Build enum ifdef map
    for enum in file_desc.enum_type:
        if enum.name in enum_usage:
            enum_ifdef_map[enum.name] = get_unique_ifdef(enum_usage[enum.name])
        else:
            enum_ifdef_map[enum.name] = None

    # Build message ifdef map
    for message in file_desc.message_type:
        # Explicit ifdef takes precedence
        explicit_ifdef = message_to_ifdef.get(message.name)
        if explicit_ifdef:
            message_ifdef_map[message.name] = explicit_ifdef
        elif message.name in message_usage:
            # Inherit ifdef if all parent messages have the same one
            if parent_ifdef := get_unique_ifdef(message_usage[message.name]):
                message_ifdef_map[message.name] = parent_ifdef
            elif message.name in message_field_ifdefs:
                # If no parent message ifdef, check if all fields using this message have the same field_ifdef
                field_ifdefs = message_field_ifdefs[message.name] - {None}
                message_ifdef_map[message.name] = (
                    field_ifdefs.pop() if len(field_ifdefs) == 1 else None
                )
            else:
                message_ifdef_map[message.name] = None
        else:
            message_ifdef_map[message.name] = None

    # Second pass: propagate ifdefs recursively
    # Keep iterating until no more changes are made
    changed = True
    iterations = 0
    while changed and iterations < 10:  # Add safety limit
        changed = False
        iterations += 1
        for message in file_desc.message_type:
            # Skip if already has an ifdef
            if message_ifdef_map.get(message.name):
                continue

            # Check if this message is used by other messages
            if message.name not in message_usage:
                continue

            # Get ifdefs from all messages that use this one
            parent_ifdefs: set[str] = {
                message_ifdef_map.get(parent)
                for parent in message_usage[message.name]
                if message_ifdef_map.get(parent)
            }

            # If all parents have the same ifdef, inherit it
            if len(parent_ifdefs) == 1 and None not in parent_ifdefs:
                message_ifdef_map[message.name] = parent_ifdefs.pop()
                changed = True

    # Build message source map
    # First pass: Get explicit sources for messages with source option or id
    for msg in file_desc.message_type:
        # Skip deprecated messages
        if msg.options.deprecated:
            continue

        if msg.options.HasExtension(pb.source):
            # Explicit source option takes precedence
            message_source_map[msg.name] = get_opt(msg, pb.source, SOURCE_BOTH)
        elif msg.options.HasExtension(pb.id):
            # Service messages (with id) default to SOURCE_BOTH
            message_source_map[msg.name] = SOURCE_BOTH
            # Service messages are always used
            used_messages.add(msg.name)

    # Second pass: Determine sources for embedded messages based on their usage
    for msg in file_desc.message_type:
        if msg.name in message_source_map:
            continue  # Already has explicit source

        if msg.name in message_usage:
            # Get sources from all parent messages that use this one
            parent_sources = {
                message_source_map[parent]
                for parent in message_usage[msg.name]
                if parent in message_source_map
            }

            # Combine parent sources
            if not parent_sources:
                # No parent has explicit source, default to encode-only
                message_source_map[msg.name] = SOURCE_SERVER
            elif len(parent_sources) > 1:
                # Multiple different sources or SOURCE_BOTH present
                message_source_map[msg.name] = SOURCE_BOTH
            else:
                # Inherit single parent source
                message_source_map[msg.name] = parent_sources.pop()
        else:
            # Not used by any message and no explicit source - default to encode-only
            message_source_map[msg.name] = SOURCE_SERVER

    return (
        enum_ifdef_map,
        message_ifdef_map,
        message_source_map,
        used_messages,
    )


def build_enum_type(desc, enum_ifdef_map) -> tuple[str, str, str]:
    """Builds the enum type.

    Args:
        desc: The enum descriptor
        enum_ifdef_map: Mapping of enum names to their ifdefs

    Returns:
        tuple: (header_content, cpp_content, dump_cpp_content)
    """
    name = desc.name

    out = f"enum {name} : uint32_t {{\n"
    for v in desc.value:
        out += f"  {v.name} = {v.number},\n"
    out += "};\n"

    # Regular cpp file has no enum content anymore
    cpp = ""

    # Dump cpp content for enum string conversion
    dump_cpp = f"template<> const char *proto_enum_to_string<enums::{name}>(enums::{name} value) {{\n"
    dump_cpp += "  switch (value) {\n"
    for v in desc.value:
        dump_cpp += f"    case enums::{v.name}:\n"
        dump_cpp += f'      return "{v.name}";\n'
    dump_cpp += "    default:\n"
    dump_cpp += '      return "UNKNOWN";\n'
    dump_cpp += "  }\n"
    dump_cpp += "}\n"

    return out, cpp, dump_cpp


def calculate_message_estimated_size(desc: descriptor.DescriptorProto) -> int:
    """Calculate estimated size for a complete message based on typical values."""
    total_size = 0

    for field in desc.field:
        # Skip deprecated fields
        if field.options.deprecated:
            continue

        ti = create_field_type_info(field)

        # Add estimated size for this field
        total_size += ti.get_estimated_size()

    return total_size


def build_message_type(
    desc: descriptor.DescriptorProto,
    base_class_fields: dict[str, list[descriptor.FieldDescriptorProto]],
    message_source_map: dict[str, int],
) -> tuple[str, str, str]:
    public_content: list[str] = []
    protected_content: list[str] = []
    decode_varint: list[str] = []
    decode_length: list[str] = []
    decode_32bit: list[str] = []
    decode_64bit: list[str] = []
    encode: list[str] = []
    dump: list[str] = []
    size_calc: list[str] = []

    # Check if this message has a base class
    base_class = get_base_class(desc)
    common_field_names = set()
    if base_class and base_class_fields and base_class in base_class_fields:
        common_field_names = {f.name for f in base_class_fields[base_class]}

    # Get message ID if it's a service message
    message_id: int | None = get_opt(desc, pb.id)

    # Get source direction to determine if we need decode/encode methods
    source = message_source_map[desc.name]
    needs_decode = source in (SOURCE_BOTH, SOURCE_CLIENT)
    needs_encode = source in (SOURCE_BOTH, SOURCE_SERVER)

    # Add MESSAGE_TYPE method if this is a service message
    if message_id is not None:
        # Validate that message_id fits in uint8_t
        if message_id > 255:
            raise ValueError(
                f"Message ID {message_id} for {desc.name} exceeds uint8_t maximum (255)"
            )

        # Add static constexpr for message type
        public_content.append(f"static constexpr uint8_t MESSAGE_TYPE = {message_id};")

        # Add estimated size constant
        estimated_size = calculate_message_estimated_size(desc)
        # Use a type appropriate for estimated_size
        estimated_size_type = (
            "uint8_t"
            if estimated_size <= 255
            else "uint16_t"
            if estimated_size <= 65535
            else "size_t"
        )
        public_content.append(
            f"static constexpr {estimated_size_type} ESTIMATED_SIZE = {estimated_size};"
        )

        # Add message_name method inline in header
        public_content.append("#ifdef HAS_PROTO_MESSAGE_DUMP")
        snake_name = camel_to_snake(desc.name)
        public_content.append(
            f'const char *message_name() const override {{ return "{snake_name}"; }}'
        )
        public_content.append("#endif")

    # Collect fixed_vector fields for custom decode generation
    fixed_vector_fields = []

    for field in desc.field:
        # Skip deprecated fields completely
        if field.options.deprecated:
            continue

        # Validate that fixed_array_size is only used in encode-only messages
        if (
            needs_decode
            and field.label == FieldDescriptorProto.LABEL_REPEATED
            and get_field_opt(field, pb.fixed_array_size) is not None
        ):
            raise ValueError(
                f"Message '{desc.name}' uses fixed_array_size on field '{field.name}' "
                f"but has source={SOURCE_NAMES[source]}. "
                f"Fixed arrays are only supported for SOURCE_SERVER (encode-only) messages "
                f"since we cannot trust or control the number of items received from clients."
            )

        # Validate that fixed_array_with_length_define is only used in encode-only messages
        if (
            needs_decode
            and field.label == FieldDescriptorProto.LABEL_REPEATED
            and get_field_opt(field, pb.fixed_array_with_length_define) is not None
        ):
            raise ValueError(
                f"Message '{desc.name}' uses fixed_array_with_length_define on field '{field.name}' "
                f"but has source={SOURCE_NAMES[source]}. "
                f"Fixed arrays with length are only supported for SOURCE_SERVER (encode-only) messages "
                f"since we cannot trust or control the number of items received from clients."
            )

        # Collect fixed_vector repeated fields for custom decode generation
        if (
            needs_decode
            and field.label == FieldDescriptorProto.LABEL_REPEATED
            and get_field_opt(field, pb.fixed_vector, False)
        ):
            fixed_vector_fields.append((field.name, field.number))

        ti = create_field_type_info(field, needs_decode, needs_encode)

        # Skip field declarations for fields that are in the base class
        # but include their encode/decode logic
        if field.name not in common_field_names:
            # Check for field_ifdef option
            field_ifdef = None
            if field.options.HasExtension(pb.field_ifdef):
                field_ifdef = field.options.Extensions[pb.field_ifdef]

            if ti.protected_content:
                protected_content.extend(
                    wrap_with_ifdef(ti.protected_content, field_ifdef)
                )
            if ti.public_content:
                public_content.extend(wrap_with_ifdef(ti.public_content, field_ifdef))

        # Only collect encode logic if this message needs it
        if needs_encode:
            # Check for field_ifdef option
            field_ifdef = None
            if field.options.HasExtension(pb.field_ifdef):
                field_ifdef = field.options.Extensions[pb.field_ifdef]

            encode.extend(wrap_with_ifdef(ti.encode_content, field_ifdef))
            size_calc.extend(
                wrap_with_ifdef(
                    ti.get_size_calculation(f"this->{ti.field_name}"), field_ifdef
                )
            )

        # Only collect decode methods if this message needs them
        if needs_decode:
            # Check for field_ifdef option for decode as well
            field_ifdef = None
            if field.options.HasExtension(pb.field_ifdef):
                field_ifdef = field.options.Extensions[pb.field_ifdef]

            if ti.decode_varint_content:
                decode_varint.extend(
                    wrap_with_ifdef(ti.decode_varint_content, field_ifdef)
                )
            if ti.decode_length_content:
                decode_length.extend(
                    wrap_with_ifdef(ti.decode_length_content, field_ifdef)
                )
            if ti.decode_32bit_content:
                decode_32bit.extend(
                    wrap_with_ifdef(ti.decode_32bit_content, field_ifdef)
                )
            if ti.decode_64bit_content:
                decode_64bit.extend(
                    wrap_with_ifdef(ti.decode_64bit_content, field_ifdef)
                )
        if ti.dump_content:
            # Check for field_ifdef option for dump as well
            field_ifdef = None
            if field.options.HasExtension(pb.field_ifdef):
                field_ifdef = field.options.Extensions[pb.field_ifdef]

            dump.extend(wrap_with_ifdef(ti.dump_content, field_ifdef))

    cpp = ""
    if decode_varint:
        o = f"bool {desc.name}::decode_varint(uint32_t field_id, ProtoVarInt value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_varint), "    ") + "\n"
        o += "    default: return false;\n"
        o += "  }\n"
        o += "  return true;\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_varint(uint32_t field_id, ProtoVarInt value) override;"
        protected_content.insert(0, prot)
    if decode_length:
        o = f"bool {desc.name}::decode_length(uint32_t field_id, ProtoLengthDelimited value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_length), "    ") + "\n"
        o += "    default: return false;\n"
        o += "  }\n"
        o += "  return true;\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;"
        protected_content.insert(0, prot)
    if decode_32bit:
        o = f"bool {desc.name}::decode_32bit(uint32_t field_id, Proto32Bit value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_32bit), "    ") + "\n"
        o += "    default: return false;\n"
        o += "  }\n"
        o += "  return true;\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_32bit(uint32_t field_id, Proto32Bit value) override;"
        protected_content.insert(0, prot)
    if decode_64bit:
        o = f"bool {desc.name}::decode_64bit(uint32_t field_id, Proto64Bit value) {{\n"
        o += "  switch (field_id) {\n"
        o += indent("\n".join(decode_64bit), "    ") + "\n"
        o += "    default: return false;\n"
        o += "  }\n"
        o += "  return true;\n"
        o += "}\n"
        cpp += o
        prot = "bool decode_64bit(uint32_t field_id, Proto64Bit value) override;"
        protected_content.insert(0, prot)

    # Generate custom decode() override for messages with FixedVector fields
    if fixed_vector_fields:
        # Generate the decode() implementation in cpp
        o = f"void {desc.name}::decode(const uint8_t *buffer, size_t length) {{\n"
        # Count and init each FixedVector field
        for field_name, field_number in fixed_vector_fields:
            o += f"  uint32_t count_{field_name} = ProtoDecodableMessage::count_repeated_field(buffer, length, {field_number});\n"
            o += f"  this->{field_name}.init(count_{field_name});\n"
        # Call parent decode to populate the fields
        o += "  ProtoDecodableMessage::decode(buffer, length);\n"
        o += "}\n"
        cpp += o
        # Generate the decode() declaration in header (public method)
        prot = "void decode(const uint8_t *buffer, size_t length) override;"
        public_content.append(prot)

    # Only generate encode method if this message needs encoding and has fields
    if needs_encode and encode:
        o = f"void {desc.name}::encode(ProtoWriteBuffer buffer) const {{"
        if len(encode) == 1 and len(encode[0]) + len(o) + 3 < 120:
            o += f" {encode[0]} }}\n"
        else:
            o += "\n"
            o += indent("\n".join(encode)) + "\n"
            o += "}\n"
        cpp += o
        prot = "void encode(ProtoWriteBuffer buffer) const override;"
        public_content.append(prot)
    # If no fields to encode or message doesn't need encoding, the default implementation in ProtoMessage will be used

    # Add calculate_size method only if this message needs encoding and has fields
    if needs_encode and size_calc:
        o = f"void {desc.name}::calculate_size(ProtoSize &size) const {{"
        # For a single field, just inline it for simplicity
        if len(size_calc) == 1 and len(size_calc[0]) + len(o) + 3 < 120:
            o += f" {size_calc[0]} }}\n"
        else:
            # For multiple fields
            o += "\n"
            o += indent("\n".join(size_calc)) + "\n"
            o += "}\n"
        cpp += o
        prot = "void calculate_size(ProtoSize &size) const override;"
        public_content.append(prot)
    # If no fields to calculate size for or message doesn't need encoding, the default implementation in ProtoMessage will be used

    # dump_to method declaration in header
    prot = "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    prot += "void dump_to(std::string &out) const override;\n"
    prot += "#endif\n"
    public_content.append(prot)

    # dump_to implementation will go in dump_cpp
    dump_impl = f"void {desc.name}::dump_to(std::string &out) const {{"
    if dump:
        if len(dump) == 1 and len(dump[0]) + len(dump_impl) + 3 < 120:
            dump_impl += f" {dump[0]} "
        else:
            dump_impl += "\n"
            dump_impl += f'  MessageDumpHelper helper(out, "{desc.name}");\n'
            dump_impl += indent("\n".join(dump)) + "\n"
    else:
        o2 = f'out.append("{desc.name} {{}}");'
        if len(dump_impl) + len(o2) + 3 < 120:
            dump_impl += f" {o2} "
        else:
            dump_impl += "\n"
            dump_impl += f"  {o2}\n"
    dump_impl += "}\n"

    if base_class:
        out = f"class {desc.name} final : public {base_class} {{\n"
    else:
        # Check if message has any non-deprecated fields
        has_fields = any(not field.options.deprecated for field in desc.field)
        # Determine inheritance based on whether the message needs decoding and has fields
        if needs_decode and has_fields:
            base_class = "ProtoDecodableMessage"
        else:
            base_class = "ProtoMessage"
        out = f"class {desc.name} final : public {base_class} {{\n"
    out += " public:\n"
    out += indent("\n".join(public_content)) + "\n"
    out += "\n"
    out += " protected:\n"
    out += indent("\n".join(protected_content))
    if len(protected_content) > 0:
        out += "\n"
    out += "};\n"

    # Build dump_cpp content with dump_to implementation
    dump_cpp = dump_impl

    return out, cpp, dump_cpp


SOURCE_BOTH = 0
SOURCE_SERVER = 1
SOURCE_CLIENT = 2

SOURCE_NAMES = {
    SOURCE_BOTH: "SOURCE_BOTH",
    SOURCE_SERVER: "SOURCE_SERVER",
    SOURCE_CLIENT: "SOURCE_CLIENT",
}

RECEIVE_CASES: dict[int, tuple[str, str | None]] = {}

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


def get_field_opt(
    field: descriptor.FieldDescriptorProto,
    opt: descriptor.FieldOptions,
    default: Any = None,
) -> Any:
    """Get the option from a field descriptor."""
    if not field.options.HasExtension(opt):
        return default
    return field.options.Extensions[opt]


def get_base_class(desc: descriptor.DescriptorProto) -> str | None:
    """Get the base_class option from a message descriptor."""
    if not desc.options.HasExtension(pb.base_class):
        return None
    return desc.options.Extensions[pb.base_class]


def collect_messages_by_base_class(
    messages: list[descriptor.DescriptorProto],
) -> dict[str, list[descriptor.DescriptorProto]]:
    """Group messages by their base_class option."""
    base_class_groups = {}

    for msg in messages:
        base_class = get_base_class(msg)
        if base_class:
            if base_class not in base_class_groups:
                base_class_groups[base_class] = []
            base_class_groups[base_class].append(msg)

    return base_class_groups


def find_common_fields(
    messages: list[descriptor.DescriptorProto],
) -> list[descriptor.FieldDescriptorProto]:
    """Find fields that are common to all messages in the list."""
    if not messages:
        return []

    # Start with fields from the first message (excluding deprecated fields)
    first_msg_fields = {
        field.name: field for field in messages[0].field if not field.options.deprecated
    }
    common_fields = []

    # Check each field to see if it exists in all messages with same type
    # Field numbers can vary between messages - derived classes handle the mapping
    for field_name, field in first_msg_fields.items():
        is_common = True

        for msg in messages[1:]:
            found = False
            for other_field in msg.field:
                # Skip deprecated fields
                if other_field.options.deprecated:
                    continue
                if (
                    other_field.name == field_name
                    and other_field.type == field.type
                    and other_field.label == field.label
                ):
                    found = True
                    break

            if not found:
                is_common = False
                break

        if is_common:
            common_fields.append(field)

    # Sort by field number to maintain order
    common_fields.sort(key=lambda f: f.number)
    return common_fields


def get_common_field_ifdef(
    field_name: str, messages: list[descriptor.DescriptorProto]
) -> str | None:
    """Get the field_ifdef option if it's consistent across all messages.

    Args:
        field_name: Name of the field to check
        messages: List of messages that contain this field

    Returns:
        The field_ifdef string if all messages have the same value, None otherwise
    """
    field_ifdefs = {
        get_field_opt(field, pb.field_ifdef)
        for msg in messages
        if (field := next((f for f in msg.field if f.name == field_name), None))
    }

    # Return the ifdef only if all messages agree on the same value
    return field_ifdefs.pop() if len(field_ifdefs) == 1 else None


def build_base_class(
    base_class_name: str,
    common_fields: list[descriptor.FieldDescriptorProto],
    messages: list[descriptor.DescriptorProto],
    message_source_map: dict[str, int],
) -> tuple[str, str, str]:
    """Build the base class definition and implementation."""
    public_content = []
    protected_content = []

    # Determine if any message using this base class needs decoding/encoding
    needs_decode = any(
        message_source_map.get(msg.name, SOURCE_BOTH) in (SOURCE_BOTH, SOURCE_CLIENT)
        for msg in messages
    )
    needs_encode = any(
        message_source_map.get(msg.name, SOURCE_BOTH) in (SOURCE_BOTH, SOURCE_SERVER)
        for msg in messages
    )

    # For base classes, we only declare the fields but don't handle encode/decode
    # The derived classes will handle encoding/decoding with their specific field numbers
    for field in common_fields:
        ti = create_field_type_info(field, needs_decode, needs_encode)

        # Get field_ifdef if it's consistent across all messages
        field_ifdef = get_common_field_ifdef(field.name, messages)

        # Only add field declarations, not encode/decode logic
        if ti.protected_content:
            protected_content.extend(wrap_with_ifdef(ti.protected_content, field_ifdef))
        if ti.public_content:
            public_content.extend(wrap_with_ifdef(ti.public_content, field_ifdef))

    # Build header
    parent_class = "ProtoDecodableMessage" if needs_decode else "ProtoMessage"
    out = f"class {base_class_name} : public {parent_class} {{\n"
    out += " public:\n"

    # Add destructor with override
    public_content.insert(0, f"~{base_class_name}() override = default;")

    # Base classes don't implement encode/decode/calculate_size
    # Derived classes handle these with their specific field numbers
    cpp = ""

    out += indent("\n".join(public_content)) + "\n"
    out += "\n"
    out += " protected:\n"
    out += indent("\n".join(protected_content))
    if protected_content:
        out += "\n"
    out += "};\n"

    # No implementation needed for base classes
    dump_cpp = ""

    return out, cpp, dump_cpp


def generate_base_classes(
    base_class_groups: dict[str, list[descriptor.DescriptorProto]],
    message_source_map: dict[str, int],
) -> tuple[str, str, str]:
    """Generate all base classes."""
    all_headers = []
    all_cpp = []
    all_dump_cpp = []

    for base_class_name, messages in base_class_groups.items():
        # Find common fields
        common_fields = find_common_fields(messages)

        if common_fields:
            # Generate base class
            header, cpp, dump_cpp = build_base_class(
                base_class_name, common_fields, messages, message_source_map
            )
            all_headers.append(header)
            all_cpp.append(cpp)
            all_dump_cpp.append(dump_cpp)

    return "\n".join(all_headers), "\n".join(all_cpp), "\n".join(all_dump_cpp)


def build_service_message_type(
    mt: descriptor.DescriptorProto,
    message_source_map: dict[str, int],
) -> tuple[str, str] | None:
    """Builds the service message type."""
    # Skip deprecated messages
    if mt.options.deprecated:
        return None

    snake = camel_to_snake(mt.name)
    id_: int | None = get_opt(mt, pb.id)
    if id_ is None:
        return None

    source: int = message_source_map.get(mt.name, SOURCE_BOTH)

    ifdef: str | None = get_opt(mt, pb.ifdef)
    log: bool = get_opt(mt, pb.log, True)
    hout = ""
    cout = ""

    # Store ifdef for later use
    if ifdef is not None:
        ifdefs[str(mt.name)] = ifdef

    if source in (SOURCE_BOTH, SOURCE_SERVER):
        # Don't generate individual send methods anymore
        # The generic send_message method will be used instead
        pass
    if source in (SOURCE_BOTH, SOURCE_CLIENT):
        # Only add ifdef when we're actually generating content
        if ifdef is not None:
            hout += f"#ifdef {ifdef}\n"
        # Generate receive
        func = f"on_{snake}"
        hout += f"virtual void {func}(const {mt.name} &value){{}};\n"
        case = ""
        case += f"{mt.name} msg;\n"
        # Check if this message has any fields (excluding deprecated ones)
        has_fields = any(not field.options.deprecated for field in mt.field)
        if has_fields:
            # Normal case: decode the message
            case += "msg.decode(msg_data, msg_size);\n"
        else:
            # Empty message optimization: skip decode since there are no fields
            case += "// Empty message: no decode needed\n"
        if log:
            case += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
            case += f'ESP_LOGVV(TAG, "{func}: %s", msg.dump().c_str());\n'
            case += "#endif\n"
        case += f"this->{func}(msg);\n"
        case += "break;"
        # Store the message name and ifdef with the case for later use
        RECEIVE_CASES[id_] = (case, ifdef, mt.name)

        # Only close ifdef if we opened it
        if ifdef is not None:
            hout += "#endif\n"

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

#include "esphome/core/defines.h"
#include "esphome/core/string_ref.h"

#include "proto.h"
#include "api_pb2_includes.h"
"""

    content += """
namespace esphome::api {

"""

    cpp = FILE_HEADER
    cpp += """\
    #include "api_pb2.h"
    #include "esphome/core/log.h"
    #include "esphome/core/helpers.h"
    #include <cstring>

namespace esphome::api {

"""

    # Initialize dump cpp content
    dump_cpp = FILE_HEADER
    dump_cpp += """\
#include "api_pb2.h"
#include "esphome/core/helpers.h"

#include <cinttypes>

#ifdef HAS_PROTO_MESSAGE_DUMP

namespace esphome::api {

// Helper function to append a quoted string, handling empty StringRef
static inline void append_quoted_string(std::string &out, const StringRef &ref) {
  out.append("'");
  if (!ref.empty()) {
    out.append(ref.c_str());
  }
  out.append("'");
}

// Common helpers for dump_field functions
static inline void append_field_prefix(std::string &out, const char *field_name, int indent) {
  out.append(indent, ' ').append(field_name).append(": ");
}

static inline void append_with_newline(std::string &out, const char *str) {
  out.append(str);
  out.append("\\n");
}

// RAII helper for message dump formatting
class MessageDumpHelper {
 public:
  MessageDumpHelper(std::string &out, const char *message_name) : out_(out) {
    out_.append(message_name);
    out_.append(" {\\n");
  }
  ~MessageDumpHelper() { out_.append(" }"); }

 private:
  std::string &out_;
};

// Helper functions to reduce code duplication in dump methods
static void dump_field(std::string &out, const char *field_name, int32_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%" PRId32, value);
  append_with_newline(out, buffer);
}

static void dump_field(std::string &out, const char *field_name, uint32_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%" PRIu32, value);
  append_with_newline(out, buffer);
}

static void dump_field(std::string &out, const char *field_name, float value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%g", value);
  append_with_newline(out, buffer);
}

static void dump_field(std::string &out, const char *field_name, uint64_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%llu", value);
  append_with_newline(out, buffer);
}

static void dump_field(std::string &out, const char *field_name, bool value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append(YESNO(value));
  out.append("\\n");
}

static void dump_field(std::string &out, const char *field_name, const std::string &value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append("'").append(value).append("'");
  out.append("\\n");
}

static void dump_field(std::string &out, const char *field_name, StringRef value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  append_quoted_string(out, value);
  out.append("\\n");
}

template<typename T>
static void dump_field(std::string &out, const char *field_name, T value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append(proto_enum_to_string<T>(value));
  out.append("\\n");
}

"""

    content += "namespace enums {\n\n"

    # Build dynamic ifdef mappings for both enums and messages
    enum_ifdef_map, message_ifdef_map, message_source_map, used_messages = (
        build_type_usage_map(file)
    )

    # Simple grouping of enums by ifdef
    current_ifdef = None

    for enum in file.enum_type:
        # Skip deprecated enums
        if enum.options.deprecated:
            continue

        s, c, dc = build_enum_type(enum, enum_ifdef_map)
        enum_ifdef = enum_ifdef_map.get(enum.name)

        # Handle ifdef changes
        if enum_ifdef != current_ifdef:
            if current_ifdef is not None:
                content += "#endif\n"
                dump_cpp += "#endif\n"
            if enum_ifdef is not None:
                content += f"#ifdef {enum_ifdef}\n"
                dump_cpp += f"#ifdef {enum_ifdef}\n"
            current_ifdef = enum_ifdef

        content += s
        cpp += c
        dump_cpp += dc

    # Close last ifdef
    if current_ifdef is not None:
        content += "#endif\n"
        dump_cpp += "#endif\n"

    content += "\n}  // namespace enums\n\n"

    mt = file.message_type

    # Collect messages by base class
    base_class_groups = collect_messages_by_base_class(mt)

    # Find common fields for each base class
    base_class_fields = {}
    for base_class_name, messages in base_class_groups.items():
        common_fields = find_common_fields(messages)
        if common_fields:
            base_class_fields[base_class_name] = common_fields

    # Generate base classes
    if base_class_fields:
        base_headers, base_cpp, base_dump_cpp = generate_base_classes(
            base_class_groups, message_source_map
        )
        content += base_headers
        cpp += base_cpp
        dump_cpp += base_dump_cpp

    # Generate message types with base class information
    # Simple grouping by ifdef
    current_ifdef = None

    for m in mt:
        # Skip deprecated messages
        if m.options.deprecated:
            continue

        # Skip messages that aren't used (unless they have an ID/service message)
        if m.name not in used_messages and not m.options.HasExtension(pb.id):
            continue

        s, c, dc = build_message_type(m, base_class_fields, message_source_map)
        msg_ifdef = message_ifdef_map.get(m.name)

        # Handle ifdef changes
        if msg_ifdef != current_ifdef:
            if current_ifdef is not None:
                content += "#endif\n"
                if cpp:
                    cpp += "#endif\n"
                if dump_cpp:
                    dump_cpp += "#endif\n"
            if msg_ifdef is not None:
                content += f"#ifdef {msg_ifdef}\n"
                cpp += f"#ifdef {msg_ifdef}\n"
                dump_cpp += f"#ifdef {msg_ifdef}\n"
            current_ifdef = msg_ifdef

        content += s
        cpp += c
        dump_cpp += dc

    # Close last ifdef
    if current_ifdef is not None:
        content += "#endif\n"
        cpp += "#endif\n"
        dump_cpp += "#endif\n"

    content += """\

}  // namespace esphome::api
"""
    cpp += """\

}  // namespace esphome::api
"""

    dump_cpp += """\

}  // namespace esphome::api

#endif  // HAS_PROTO_MESSAGE_DUMP
"""

    with open(root / "api_pb2.h", "w", encoding="utf-8") as f:
        f.write(content)

    with open(root / "api_pb2.cpp", "w", encoding="utf-8") as f:
        f.write(cpp)

    with open(root / "api_pb2_dump.cpp", "w", encoding="utf-8") as f:
        f.write(dump_cpp)

    hpp = FILE_HEADER
    hpp += """\
#pragma once

#include "esphome/core/defines.h"

#include "api_pb2.h"

namespace esphome::api {

"""

    cpp = FILE_HEADER
    cpp += """\
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome::api {

static const char *const TAG = "api.service";

"""

    class_name = "APIServerConnectionBase"

    hpp += f"class {class_name} : public ProtoService {{\n"
    hpp += " public:\n"

    # Add logging helper method declaration
    hpp += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    hpp += " protected:\n"
    hpp += "  void log_send_message_(const char *name, const std::string &dump);\n"
    hpp += " public:\n"
    hpp += "#endif\n\n"

    # Add non-template send_message method
    hpp += "  bool send_message(const ProtoMessage &msg, uint8_t message_type) {\n"
    hpp += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    hpp += "    this->log_send_message_(msg.message_name(), msg.dump());\n"
    hpp += "#endif\n"
    hpp += "    return this->send_message_(msg, message_type);\n"
    hpp += "  }\n\n"

    # Add logging helper method implementation to cpp
    cpp += "#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    cpp += f"void {class_name}::log_send_message_(const char *name, const std::string &dump) {{\n"
    cpp += '  ESP_LOGVV(TAG, "send_message %s: %s", name, dump.c_str());\n'
    cpp += "}\n"
    cpp += "#endif\n\n"

    for mt in file.message_type:
        obj = build_service_message_type(mt, message_source_map)
        if obj is None:
            continue
        hout, cout = obj
        hpp += indent(hout) + "\n"
        cpp += cout

    cases = list(RECEIVE_CASES.items())
    cases.sort()
    hpp += " protected:\n"
    hpp += "  void read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;\n"
    out = f"void {class_name}::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {{\n"
    out += "  switch (msg_type) {\n"
    for i, (case, ifdef, message_name) in cases:
        if ifdef is not None:
            out += f"#ifdef {ifdef}\n"

        c = f"    case {message_name}::MESSAGE_TYPE: {{\n"
        c += indent(case, "      ") + "\n"
        c += "    }"
        out += c + "\n"
        if ifdef is not None:
            out += "#endif\n"
    out += "    default:\n"
    out += "      break;\n"
    out += "  }\n"
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

    # Build a mapping of message input types to their authentication requirements
    message_auth_map: dict[str, bool] = {}
    message_conn_map: dict[str, bool] = {}

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

        # Store authentication requirements for message types
        message_auth_map[inp] = needs_auth
        message_conn_map[inp] = needs_conn

        ifdef = message_ifdef_map.get(inp, ifdefs.get(inp))

        if ifdef is not None:
            hpp += f"#ifdef {ifdef}\n"
            hpp_protected += f"#ifdef {ifdef}\n"
            cpp += f"#ifdef {ifdef}\n"

        hpp_protected += f"  void {on_func}(const {inp} &msg) override;\n"

        # For non-void methods, generate a send_ method instead of return-by-value
        if is_void:
            hpp += f"  virtual void {func}(const {inp} &msg) = 0;\n"
        else:
            hpp += f"  virtual bool send_{func}_response(const {inp} &msg) = 0;\n"

        cpp += f"void {class_name}::{on_func}(const {inp} &msg) {{\n"

        # No authentication check here - it's done in read_message
        body = ""
        if is_void:
            body += f"this->{func}(msg);\n"
        else:
            body += f"if (!this->send_{func}_response(msg)) {{\n"
            body += "  this->on_fatal_error();\n"
            body += "}\n"

        cpp += indent(body) + "\n" + "}\n"

        if ifdef is not None:
            hpp += "#endif\n"
            hpp_protected += "#endif\n"
            cpp += "#endif\n"

    # Generate optimized read_message with authentication checking
    # Categorize messages by their authentication requirements
    no_conn_ids: set[int] = set()
    conn_only_ids: set[int] = set()

    for id_, (_, _, case_msg_name) in cases:
        if case_msg_name in message_auth_map:
            needs_auth = message_auth_map[case_msg_name]
            needs_conn = message_conn_map[case_msg_name]

            if not needs_conn:
                no_conn_ids.add(id_)
            elif not needs_auth:
                conn_only_ids.add(id_)

    # Generate override if we have messages that skip checks
    if no_conn_ids or conn_only_ids:
        # Helper to generate case statements with ifdefs
        def generate_cases(ids: set[int], comment: str) -> str:
            result = ""
            for id_ in sorted(ids):
                _, ifdef, msg_name = RECEIVE_CASES[id_]
                if ifdef:
                    result += f"#ifdef {ifdef}\n"
                result += f"    case {msg_name}::MESSAGE_TYPE:  {comment}\n"
                if ifdef:
                    result += "#endif\n"
            return result

        hpp_protected += "  void read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;\n"

        cpp += f"\nvoid {class_name}::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {{\n"
        cpp += "  // Check authentication/connection requirements for messages\n"
        cpp += "  switch (msg_type) {\n"

        # Messages that don't need any checks
        if no_conn_ids:
            cpp += generate_cases(no_conn_ids, "// No setup required")
            cpp += "      break;  // Skip all checks for these messages\n"

        # Messages that only need connection setup
        if conn_only_ids:
            cpp += generate_cases(conn_only_ids, "// Connection setup only")
            cpp += "      if (!this->check_connection_setup_()) {\n"
            cpp += "        return;  // Connection not setup\n"
            cpp += "      }\n"
            cpp += "      break;\n"

        cpp += "    default:\n"
        cpp += "      // All other messages require authentication (which includes connection check)\n"
        cpp += "      if (!this->check_authenticated_()) {\n"
        cpp += "        return;  // Authentication failed\n"
        cpp += "      }\n"
        cpp += "      break;\n"
        cpp += "  }\n\n"
        cpp += "  // Call base implementation to process the message\n"
        cpp += f"  {class_name}Base::read_message(msg_size, msg_type, msg_data);\n"
        cpp += "}\n"

    hpp += " protected:\n"
    hpp += hpp_protected
    hpp += "};\n"

    hpp += """\

}  // namespace esphome::api
"""
    cpp += """\

}  // namespace esphome::api
"""

    with open(root / "api_pb2_service.h", "w", encoding="utf-8") as f:
        f.write(hpp)

    with open(root / "api_pb2_service.cpp", "w", encoding="utf-8") as f:
        f.write(cpp)

    prot_file.unlink()

    try:
        import clang_format

        def exec_clang_format(path: Path) -> None:
            clang_format_path = (
                Path(clang_format.__file__).parent / "data" / "bin" / "clang-format"
            )
            call([clang_format_path, "-i", path])

        exec_clang_format(root / "api_pb2_service.h")
        exec_clang_format(root / "api_pb2_service.cpp")
        exec_clang_format(root / "api_pb2.h")
        exec_clang_format(root / "api_pb2.cpp")
        exec_clang_format(root / "api_pb2_dump.cpp")
    except ImportError:
        pass


if __name__ == "__main__":
    sys.exit(main())
