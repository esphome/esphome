#!/usr/bin/env python3
"""Python 3 script to automatically generate C++ classes for ESPHome's native API.

It's pretty crappy spaghetti code, but it works.

you need to install protobuf-compiler:
running protc --version should return
libprotoc 3.6.1

then run this script with python3 and the files

    esphome/components/api/api_pb2_service.h
    esphome/components/api/api_pb2_service.cpp
    esphome/components/api/api_pb2.h
    esphome/components/api/api_pb2.cpp

will be generated, they still need to be formatted
"""

import re
import os
from pathlib import Path
from textwrap import dedent
from subprocess import call

# Generate with
# protoc --python_out=script/api_protobuf -I esphome/components/api/ api_options.proto

import aioesphomeapi.api_options_pb2 as pb
import google.protobuf.descriptor_pb2 as descriptor

file_header = "// This file was automatically generated with a tool.\n"
file_header += "// See scripts/api_protobuf/api_protobuf.py\n"

cwd = Path(__file__).resolve().parent
root = cwd.parent.parent / "esphome" / "components" / "api"
prot = root / "api.protoc"
call(["protoc", "-o", str(prot), "-I", str(root), "api.proto"])
content = prot.read_bytes()

d = descriptor.FileDescriptorSet.FromString(content)


def indent_list(text, padding="  "):
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


def indent(text, padding="  "):
    return "\n".join(indent_list(text, padding))


def camel_to_snake(name):
    # https://stackoverflow.com/a/1176023
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s1).lower()


class TypeInfo:
    def __init__(self, field):
        self._field = field

    @property
    def default_value(self):
        return ""

    @property
    def name(self):
        return self._field.name

    @property
    def arg_name(self):
        return self.name

    @property
    def field_name(self):
        return self.name

    @property
    def number(self):
        return self._field.number

    @property
    def repeated(self):
        return self._field.label == 3

    @property
    def cpp_type(self):
        raise NotImplementedError

    @property
    def reference_type(self):
        return f"{self.cpp_type} "

    @property
    def const_reference_type(self):
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
    def encode_content(self):
        return f"buffer.{self.encode_func}({self.number}, this->{self.field_name});"

    encode_func = None

    @property
    def dump_content(self):
        o = f'out.append("  {self.name}: ");\n'
        o += self.dump(f"this->{self.field_name}") + "\n"
        o += f'out.append("\\n");\n'
        return o

    dump = None


TYPE_INFO = {}


def register_type(name):
    def func(value):
        TYPE_INFO[name] = value
        return value

    return func


@register_type(1)
class DoubleType(TypeInfo):
    cpp_type = "double"
    default_value = "0.0"
    decode_64bit = "value.as_double()"
    encode_func = "encode_double"

    def dump(self, name):
        o = f'sprintf(buffer, "%g", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(2)
class FloatType(TypeInfo):
    cpp_type = "float"
    default_value = "0.0f"
    decode_32bit = "value.as_float()"
    encode_func = "encode_float"

    def dump(self, name):
        o = f'sprintf(buffer, "%g", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(3)
class Int64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_int64()"
    encode_func = "encode_int64"

    def dump(self, name):
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(4)
class UInt64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_varint = "value.as_uint64()"
    encode_func = "encode_uint64"

    def dump(self, name):
        o = f'sprintf(buffer, "%llu", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(5)
class Int32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_int32()"
    encode_func = "encode_int32"

    def dump(self, name):
        o = f'sprintf(buffer, "%d", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(6)
class Fixed64Type(TypeInfo):
    cpp_type = "uint64_t"
    default_value = "0"
    decode_64bit = "value.as_fixed64()"
    encode_func = "encode_fixed64"

    def dump(self, name):
        o = f'sprintf(buffer, "%llu", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(7)
class Fixed32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_32bit = "value.as_fixed32()"
    encode_func = "encode_fixed32"

    def dump(self, name):
        o = f'sprintf(buffer, "%u", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(8)
class BoolType(TypeInfo):
    cpp_type = "bool"
    default_value = "false"
    decode_varint = "value.as_bool()"
    encode_func = "encode_bool"

    def dump(self, name):
        o = f"out.append(YESNO({name}));"
        return o


@register_type(9)
class StringType(TypeInfo):
    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    decode_length = "value.as_string()"
    encode_func = "encode_string"

    def dump(self, name):
        o = f'out.append("\'").append({name}).append("\'");'
        return o


@register_type(11)
class MessageType(TypeInfo):
    @property
    def cpp_type(self):
        return self._field.type_name[1:]

    default_value = ""

    @property
    def reference_type(self):
        return f"{self.cpp_type} &"

    @property
    def const_reference_type(self):
        return f"const {self.cpp_type} &"

    @property
    def encode_func(self):
        return f"encode_message<{self.cpp_type}>"

    @property
    def decode_length(self):
        return f"value.as_message<{self.cpp_type}>()"

    def dump(self, name):
        o = f"{name}.dump_to(out);"
        return o


@register_type(12)
class BytesType(TypeInfo):
    cpp_type = "std::string"
    default_value = ""
    reference_type = "std::string &"
    const_reference_type = "const std::string &"
    decode_length = "value.as_string()"
    encode_func = "encode_string"

    def dump(self, name):
        o = f'out.append("\'").append({name}).append("\'");'
        return o


@register_type(13)
class UInt32Type(TypeInfo):
    cpp_type = "uint32_t"
    default_value = "0"
    decode_varint = "value.as_uint32()"
    encode_func = "encode_uint32"

    def dump(self, name):
        o = f'sprintf(buffer, "%u", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(14)
class EnumType(TypeInfo):
    @property
    def cpp_type(self):
        return f"enums::{self._field.type_name[1:]}"

    @property
    def decode_varint(self):
        return f"value.as_enum<{self.cpp_type}>()"

    default_value = ""

    @property
    def encode_func(self):
        return f"encode_enum<{self.cpp_type}>"

    def dump(self, name):
        o = f"out.append(proto_enum_to_string<{self.cpp_type}>({name}));"
        return o


@register_type(15)
class SFixed32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_32bit = "value.as_sfixed32()"
    encode_func = "encode_sfixed32"

    def dump(self, name):
        o = f'sprintf(buffer, "%d", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(16)
class SFixed64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_64bit = "value.as_sfixed64()"
    encode_func = "encode_sfixed64"

    def dump(self, name):
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(17)
class SInt32Type(TypeInfo):
    cpp_type = "int32_t"
    default_value = "0"
    decode_varint = "value.as_sint32()"
    encode_func = "encode_sint32"

    def dump(self, name):
        o = f'sprintf(buffer, "%d", {name});\n'
        o += f"out.append(buffer);"
        return o


@register_type(18)
class SInt64Type(TypeInfo):
    cpp_type = "int64_t"
    default_value = "0"
    decode_varint = "value.as_sint64()"
    encode_func = "encode_sint64"

    def dump(self, name):
        o = f'sprintf(buffer, "%lld", {name});\n'
        o += f"out.append(buffer);"
        return o


class RepeatedTypeInfo(TypeInfo):
    def __init__(self, field):
        super().__init__(field)
        self._ti = TYPE_INFO[field.type](field)

    @property
    def cpp_type(self):
        return f"std::vector<{self._ti.cpp_type}>"

    @property
    def reference_type(self):
        return f"{self.cpp_type} &"

    @property
    def const_reference_type(self):
        return f"const {self.cpp_type} &"

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
    def _ti_is_bool(self):
        # std::vector is specialized for bool, reference does not work
        return isinstance(self._ti, BoolType)

    @property
    def encode_content(self):
        o = f"for (auto {'' if self._ti_is_bool else '&'}it : this->{self.field_name}) {{\n"
        o += f"  buffer.{self._ti.encode_func}({self.number}, it, true);\n"
        o += f"}}"
        return o

    @property
    def dump_content(self):
        o = f'for (const auto {"" if self._ti_is_bool else "&"}it : this->{self.field_name}) {{\n'
        o += f'  out.append("  {self.name}: ");\n'
        o += indent(self._ti.dump("it")) + "\n"
        o += f'  out.append("\\n");\n'
        o += f"}}\n"
        return o


def build_enum_type(desc):
    name = desc.name
    out = f"enum {name} : uint32_t {{\n"
    for v in desc.value:
        out += f"  {v.name} = {v.number},\n"
    out += "};\n"

    cpp = f"#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    cpp += f"template<> const char *proto_enum_to_string<enums::{name}>(enums::{name} value) {{\n"
    cpp += f"  switch (value) {{\n"
    for v in desc.value:
        cpp += f"    case enums::{v.name}:\n"
        cpp += f'      return "{v.name}";\n'
    cpp += f"    default:\n"
    cpp += f'      return "UNKNOWN";\n'
    cpp += f"  }}\n"
    cpp += f"}}\n"
    cpp += f"#endif\n"

    return out, cpp


def build_message_type(desc):
    public_content = []
    protected_content = []
    decode_varint = []
    decode_length = []
    decode_32bit = []
    decode_64bit = []
    encode = []
    dump = []

    for field in desc.field:
        if field.label == 3:
            ti = RepeatedTypeInfo(field)
        else:
            ti = TYPE_INFO[field.type](field)
        protected_content.extend(ti.protected_content)
        public_content.extend(ti.public_content)
        encode.append(ti.encode_content)

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

    o = f"void {desc.name}::dump_to(std::string &out) const {{"
    if dump:
        if len(dump) == 1 and len(dump[0]) + len(o) + 3 < 120:
            o += f" {dump[0]} "
        else:
            o += "\n"
            o += f"  __attribute__((unused)) char buffer[64];\n"
            o += f'  out.append("{desc.name} {{\\n");\n'
            o += indent("\n".join(dump)) + "\n"
            o += f'  out.append("}}");\n'
    else:
        o2 = f'out.append("{desc.name} {{}}");'
        if len(o) + len(o2) + 3 < 120:
            o += f" {o2} "
        else:
            o += "\n"
            o += f"  {o2}\n"
    o += "}\n"
    cpp += f"#ifdef HAS_PROTO_MESSAGE_DUMP\n"
    cpp += o
    cpp += f"#endif\n"
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


file = d.file[0]
content = file_header
content += """\
#pragma once

#include "proto.h"

namespace esphome {
namespace api {

"""

cpp = file_header
cpp += """\
#include "api_pb2.h"
#include "esphome/core/log.h"

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

with open(root / "api_pb2.h", "w") as f:
    f.write(content)

with open(root / "api_pb2.cpp", "w") as f:
    f.write(cpp)

SOURCE_BOTH = 0
SOURCE_SERVER = 1
SOURCE_CLIENT = 2

RECEIVE_CASES = {}

class_name = "APIServerConnectionBase"

ifdefs = {}


def get_opt(desc, opt, default=None):
    if not desc.options.HasExtension(opt):
        return default
    return desc.options.Extensions[opt]


def build_service_message_type(mt):
    snake = camel_to_snake(mt.name)
    id_ = get_opt(mt, pb.id)
    if id_ is None:
        return None

    source = get_opt(mt, pb.source, 0)

    ifdef = get_opt(mt, pb.ifdef)
    log = get_opt(mt, pb.log, True)
    nodelay = get_opt(mt, pb.no_delay, False)
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
        cout += f"bool {class_name}::{func}(const {mt.name} &msg) {{\n"
        if log:
            cout += f"#ifdef HAS_PROTO_MESSAGE_DUMP\n"
            cout += f'  ESP_LOGVV(TAG, "{func}: %s", msg.dump().c_str());\n'
            cout += f"#endif\n"
        # cout += f'  this->set_nodelay({str(nodelay).lower()});\n'
        cout += f"  return this->send_message_<{mt.name}>(msg, {id_});\n"
        cout += f"}}\n"
    if source in (SOURCE_BOTH, SOURCE_CLIENT):
        # Generate receive
        func = f"on_{snake}"
        hout += f"virtual void {func}(const {mt.name} &value){{}};\n"
        case = ""
        if ifdef is not None:
            case += f"#ifdef {ifdef}\n"
        case += f"{mt.name} msg;\n"
        case += f"msg.decode(msg_data, msg_size);\n"
        if log:
            case += f"#ifdef HAS_PROTO_MESSAGE_DUMP\n"
            case += f'ESP_LOGVV(TAG, "{func}: %s", msg.dump().c_str());\n'
            case += f"#endif\n"
        case += f"this->{func}(msg);\n"
        if ifdef is not None:
            case += f"#endif\n"
        case += "break;"
        RECEIVE_CASES[id_] = case

    if ifdef is not None:
        hout += f"#endif\n"
        cout += f"#endif\n"

    return hout, cout


hpp = file_header
hpp += """\
#pragma once

#include "api_pb2.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

"""

cpp = file_header
cpp += """\
#include "api_pb2_service.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.service";

"""

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
hpp += f"  bool read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) override;\n"
out = f"bool {class_name}::read_message(uint32_t msg_size, uint32_t msg_type, uint8_t *msg_data) {{\n"
out += f"  switch (msg_type) {{\n"
for i, case in cases:
    c = f"case {i}: {{\n"
    c += indent(case) + "\n"
    c += f"}}"
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
        body += f"  this->on_fatal_error();\n"
        body += "}\n"
    cpp += indent(body) + "\n" + "}\n"

    if ifdef is not None:
        hpp += f"#endif\n"
        hpp_protected += f"#endif\n"
        cpp += f"#endif\n"

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

with open(root / "api_pb2_service.h", "w") as f:
    f.write(hpp)

with open(root / "api_pb2_service.cpp", "w") as f:
    f.write(cpp)

prot.unlink()

try:
    import clang_format

    def exec_clang_format(path):
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
