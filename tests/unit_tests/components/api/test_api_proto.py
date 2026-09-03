"""Invariant tests for esphome/components/api/api.proto and its generated code.

These guard the DeviceCapabilitiesRequest/DeviceCapabilitiesResponse addition
(API 1.15) against regressions that protoc-based codegen would not catch on
its own, without requiring protoc to be installed at test time:

* script/api_protobuf/api_protobuf.py skips any field marked
  `[deprecated = true]` completely -- it generates no C++ for it at all, so
  the device silently stops sending that value. Six DeviceInfoResponse fields
  were superseded by DeviceCapabilitiesResponse but must keep being sent for
  backward compatibility with clients older than API 1.15. If a future edit
  "tidies up" by marking one of them deprecated, this file breaks that field
  for every existing client with nothing else in CI noticing.
* Field numbers are the wire protocol, not the field names. Renaming a field
  is harmless; renumbering it is a silent breaking change, because an old
  client still decodes by number. This file pins the field number of each of
  the six superseded DeviceInfoResponse fields and of every field on the new
  DeviceCapabilitiesResponse/BluetoothProxyCapabilities/
  VoiceAssistantCapabilities/ZWaveProxyCapabilities sub-messages, so a
  well-intentioned reshuffle of api.proto gets caught here instead of on a
  device in the field.
* Message wire ids must be unique, and the new capabilities RPC must stay
  authenticated-only.

Group A below asserts on the checked-in generated files (api_pb2.h /
api_pb2.cpp), since "the field is present in the generated C++" is exactly
equivalent to "the device still sends it". Group B parses api.proto as plain
text (no protoc). Group C checks the advertised API minor version.
"""

from __future__ import annotations

from pathlib import Path
import re

import esphome

API_DIR = Path(esphome.__file__).parent / "components" / "api"

PROTO_TEXT = (API_DIR / "api.proto").read_text(encoding="utf-8")
HEADER_TEXT = (API_DIR / "api_pb2.h").read_text(encoding="utf-8")
CPP_TEXT = (API_DIR / "api_pb2.cpp").read_text(encoding="utf-8")
API_CONNECTION_TEXT = (API_DIR / "api_connection.cpp").read_text(encoding="utf-8")

# Fields on DeviceInfoResponse that were superseded by DeviceCapabilitiesResponse
# as of API 1.15 but must still be generated (and therefore still sent) for
# backward compatibility with older clients.
SUPERSEDED_FIELDS: dict[str, int] = {
    "bluetooth_proxy_feature_flags": 15,
    "voice_assistant_feature_flags": 17,
    "bluetooth_mac_address": 18,
    "zwave_proxy_feature_flags": 23,
    "zwave_home_id": 24,
    "serial_proxies": 25,
}

# Field numbers on the new capability messages. These are a frozen wire
# contract from the moment they ship: an old client decodes a sub-message
# field purely by number, so renumbering any of these -- even without
# touching a name -- silently corrupts what every already-deployed client
# reads. Keyed by message name so the next capability sub-message is a
# data-only addition here.
NEW_CAPABILITY_FIELDS: dict[str, dict[str, int]] = {
    "DeviceCapabilitiesResponse": {
        "bluetooth_proxy": 1,
        "voice_assistant": 2,
        "zwave_proxy": 3,
        "serial_proxies": 4,
    },
    "BluetoothProxyCapabilities": {
        "feature_flags": 1,
        "mac_address": 2,
    },
    "VoiceAssistantCapabilities": {
        "feature_flags": 1,
    },
    "ZWaveProxyCapabilities": {
        "feature_flags": 1,
        "home_id": 2,
    },
}

# Fields that are genuinely dead and are expected to carry `deprecated=true`.
# Used to prove the deprecated-detection logic below actually detects
# deprecation rather than trivially passing.
GENUINELY_DEPRECATED_FIELDS: tuple[str, ...] = (
    "legacy_bluetooth_proxy_version",
    "legacy_voice_assistant_version",
)

DEPRECATED_FIELD_TRAP = (
    "script/api_protobuf/api_protobuf.py skips fields marked `[deprecated = "
    "true]` completely, generating no C++ for them at all. Marking this field "
    "deprecated would silently stop the device from ever sending it, breaking "
    "every existing client that still reads it from DeviceInfoResponse."
)


def _extract_braced_region(text: str, anchor_pattern: str) -> str:
    """Return the region of `text` starting at the first match of
    `anchor_pattern` up to the matching closing brace (inclusive), using
    brace-depth counting so nested braces (e.g. a `for (...) { ... }` loop
    inside a function body) don't cause a premature stop.
    """
    anchor_match = re.search(anchor_pattern, text)
    if anchor_match is None:
        raise AssertionError(f"could not find a match for {anchor_pattern!r}")
    start = anchor_match.start()
    open_brace = text.index("{", start)
    depth = 0
    for i in range(open_brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
    raise AssertionError(f"unbalanced braces while scanning after {anchor_pattern!r}")


def _extract_class_body(header_text: str, class_name: str) -> str:
    """Return the body of a generated C++ class, scoped so a field name that
    also happens to exist on some other class cannot satisfy the assertion.
    """
    return _extract_braced_region(header_text, rf"class {re.escape(class_name)}\b")


def _extract_function_body(cpp_text: str, qualified_name: str) -> str:
    """Return the body of a generated `Class::method(...)` definition."""
    return _extract_braced_region(cpp_text, rf"{re.escape(qualified_name)}\(")


def _extract_proto_message(proto_text: str, message_name: str) -> str:
    """Return the body of a top-level `message Name { ... }` block from the
    .proto source. Proto message bodies here contain no nested `{`/`}` of
    their own (options use parens, not braces), so a non-greedy match up to
    the first line that is just `}` is sufficient and keeps the parsing
    simple.
    """
    match = re.search(
        rf"^message {re.escape(message_name)}\s*\{{(.*?)^\}}",
        proto_text,
        re.MULTILINE | re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"could not find `message {message_name}` in api.proto")
    return match.group(1)


def _extract_rpc_body(proto_text: str, rpc_name: str) -> str:
    """Return the option body of an `rpc name (...) returns (...) { ... }`
    declaration from the APIConnection service, robust to it being written
    on one line (`{}`) or spread across several with options inside.
    """
    match = re.search(
        rf"rpc\s+{re.escape(rpc_name)}\s*\([^)]*\)\s*returns\s*\([^)]*\)\s*\{{(.*?)\}}",
        proto_text,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"could not find `rpc {rpc_name}` in api.proto")
    return match.group(1)


def _field_declaration_line(message_body: str, field_name: str) -> str:
    """Return the single source line declaring `field_name` inside a proto
    message body (all fields here are declared on one line).
    """
    for line in message_body.splitlines():
        if re.search(rf"\b{re.escape(field_name)}\s*=\s*\d+", line):
            return line
    raise AssertionError(
        f"could not find a field declaration for {field_name!r} in the given message body"
    )


# ==================== Group A: generated files ====================


def test_superseded_device_info_fields_still_declared_in_header() -> None:
    """Each superseded field must still be a real member of DeviceInfoResponse
    in api_pb2.h -- not merely present somewhere in the file. Several of these
    names (e.g. serial_proxies) also exist on DeviceCapabilitiesResponse, so an
    unscoped substring search over the whole header would pass even if the
    field were removed from DeviceInfoResponse.
    """
    class_body = _extract_class_body(HEADER_TEXT, "DeviceInfoResponse")
    for field_name in SUPERSEDED_FIELDS:
        assert re.search(rf"\b{field_name}\b", class_body), (
            f"{field_name} is missing from the DeviceInfoResponse class body in "
            f"api_pb2.h. {DEPRECATED_FIELD_TRAP}"
        )


def test_superseded_device_info_fields_still_encoded_and_sized() -> None:
    """Each superseded field must still be touched by DeviceInfoResponse's
    generated encode() and calculate_size(), i.e. it is still put on the wire.
    """
    encode_body = _extract_function_body(CPP_TEXT, "DeviceInfoResponse::encode")
    size_body = _extract_function_body(CPP_TEXT, "DeviceInfoResponse::calculate_size")
    for field_name in SUPERSEDED_FIELDS:
        assert f"this->{field_name}" in encode_body, (
            f"DeviceInfoResponse::encode() no longer references {field_name}. "
            f"{DEPRECATED_FIELD_TRAP}"
        )
        assert f"this->{field_name}" in size_body, (
            f"DeviceInfoResponse::calculate_size() no longer references "
            f"{field_name}. {DEPRECATED_FIELD_TRAP}"
        )


def test_new_capability_classes_present_in_header() -> None:
    """The new response message and its capability sub-messages must exist as
    generated classes.
    """
    for class_name in (
        "DeviceCapabilitiesResponse",
        "BluetoothProxyCapabilities",
        "VoiceAssistantCapabilities",
        "ZWaveProxyCapabilities",
    ):
        assert re.search(rf"class {re.escape(class_name)}\b", HEADER_TEXT), (
            f"expected a generated class named {class_name} in api_pb2.h"
        )


# ==================== Group B: api.proto source text ====================


def test_all_message_ids_are_unique() -> None:
    """Every `option (id) = N;` in api.proto must be unique. Two messages
    sharing a wire id would make the client and server misinterpret each
    other's messages -- nothing else currently checks this.
    """
    ids = [int(value) for value in re.findall(r"option \(id\) = (\d+);", PROTO_TEXT)]
    assert ids, "did not find any `option (id) = N;` declarations in api.proto"
    duplicates = sorted({value for value in ids if ids.count(value) > 1})
    assert not duplicates, (
        f"Duplicate `option (id)` values found in api.proto: {duplicates}. Each "
        "message must have a unique wire id."
    )


def test_device_capabilities_request_has_id_149() -> None:
    body = _extract_proto_message(PROTO_TEXT, "DeviceCapabilitiesRequest")
    match = re.search(r"option \(id\) = (\d+);", body)
    assert match is not None, "DeviceCapabilitiesRequest is missing `option (id)`"
    assert int(match.group(1)) == 149, (
        f"DeviceCapabilitiesRequest has id {match.group(1)}, expected 149. "
        "Message ids are part of the wire protocol and must not change once "
        "assigned."
    )


def test_device_capabilities_response_has_id_150() -> None:
    body = _extract_proto_message(PROTO_TEXT, "DeviceCapabilitiesResponse")
    match = re.search(r"option \(id\) = (\d+);", body)
    assert match is not None, "DeviceCapabilitiesResponse is missing `option (id)`"
    assert int(match.group(1)) == 150, (
        f"DeviceCapabilitiesResponse has id {match.group(1)}, expected 150. "
        "Message ids are part of the wire protocol and must not change once "
        "assigned."
    )


def test_z_wave_proxy_request_response_has_id_151() -> None:
    body = _extract_proto_message(PROTO_TEXT, "ZWaveProxyRequestResponse")
    match = re.search(r"option \(id\) = (\d+);", body)
    assert match is not None, "ZWaveProxyRequestResponse is missing `option (id)`"
    assert int(match.group(1)) == 151, (
        f"ZWaveProxyRequestResponse has id {match.group(1)}, expected 151. "
        "Message ids are part of the wire protocol and must not change once "
        "assigned."
    )


def test_superseded_fields_are_not_marked_deprecated_in_proto() -> None:
    """The six superseded fields must not carry `[deprecated = true]` in
    api.proto, or the generator drops them and old clients stop receiving
    them (see module docstring). The second half of this test proves the
    deprecated-detection itself works: two genuinely dead fields
    (legacy_bluetooth_proxy_version, legacy_voice_assistant_version) must
    still be detected as deprecated, so the first half isn't vacuously true.
    """
    body = _extract_proto_message(PROTO_TEXT, "DeviceInfoResponse")

    for field_name in SUPERSEDED_FIELDS:
        line = _field_declaration_line(body, field_name)
        assert "deprecated" not in line, (
            f"{field_name} in DeviceInfoResponse is marked deprecated in "
            f"api.proto ({line.strip()!r}). {DEPRECATED_FIELD_TRAP}"
        )

    for field_name in GENUINELY_DEPRECATED_FIELDS:
        line = _field_declaration_line(body, field_name)
        assert "deprecated" in line, (
            f"expected {field_name} to still carry `deprecated=true` in "
            f"api.proto ({line.strip()!r}). If this fails, the deprecated "
            "detection used above is broken, and the sibling assertion that "
            "the superseded fields are NOT deprecated is not testing anything."
        )


def test_superseded_fields_keep_their_wire_numbers() -> None:
    """Each superseded field must stay on the field number recorded in
    SUPERSEDED_FIELDS. Old clients decode DeviceInfoResponse purely by field
    number, so renumbering one of these -- even without touching its name --
    would make an old client read a completely different value out of the
    wire, with nothing else in CI noticing.
    """
    body = _extract_proto_message(PROTO_TEXT, "DeviceInfoResponse")

    for field_name, field_number in SUPERSEDED_FIELDS.items():
        line = _field_declaration_line(body, field_name)
        assert re.search(rf"\b{field_name}\s*=\s*{field_number}\b", line), (
            f"{field_name} in DeviceInfoResponse is no longer declared at "
            f"field number {field_number} ({line.strip()!r}). Field numbers "
            "are the wire protocol -- renumbering this field silently breaks "
            "every existing client that still decodes DeviceInfoResponse by "
            "the old numbering."
        )


def test_capability_message_fields_keep_their_wire_numbers() -> None:
    """Every field on DeviceCapabilitiesResponse and its three capability
    sub-messages must stay on the field number recorded in
    NEW_CAPABILITY_FIELDS. These messages are brand new as of API 1.15, but
    the moment a device ships with them, their field numbers are a frozen
    wire contract -- a client decodes a sub-message field purely by number,
    so a later "cleanup" that renumbers one of these would silently corrupt
    what every already-deployed client reads, with nothing else in CI
    noticing.
    """
    for message_name, fields in NEW_CAPABILITY_FIELDS.items():
        body = _extract_proto_message(PROTO_TEXT, message_name)
        for field_name, field_number in fields.items():
            line = _field_declaration_line(body, field_name)
            assert re.search(rf"\b{field_name}\s*=\s*{field_number}\b", line), (
                f"{field_name} on {message_name} is no longer declared at "
                f"field number {field_number} ({line.strip()!r}). Field "
                "numbers are the wire protocol -- renumbering this field "
                "silently breaks every existing client that decodes this "
                "message by the old numbering."
            )


def test_device_capabilities_rpc_requires_authentication() -> None:
    """The `device_capabilities` RPC must not set
    `option (needs_authentication) = false;` (or set it to anything at all).
    Leaving it unset makes it inherit needs_authentication = true, keeping
    capability data behind authentication (and encryption, when configured).
    """
    body = _extract_rpc_body(PROTO_TEXT, "device_capabilities")
    assert "needs_authentication" not in body, (
        "rpc device_capabilities sets a `needs_authentication` option in "
        "api.proto. It must stay unset so it inherits needs_authentication = "
        "true; otherwise device capability data could be requested over an "
        "unauthenticated connection."
    )


# ==================== Group C: advertised API version ====================


def test_api_version_minor_is_at_least_15() -> None:
    """Clients gate sending DeviceCapabilitiesRequest on seeing
    api_version >= 1.15 in HelloResponse. Regressing api_version_minor below
    15 would make every client believe capabilities are unsupported even
    though the RPC exists, so this must never go backwards. Use >= rather
    than == so the next unrelated minor-version bump doesn't need to touch
    this test.
    """
    match = re.search(r"resp\.api_version_minor\s*=\s*(\d+);", API_CONNECTION_TEXT)
    assert match is not None, (
        "could not find `resp.api_version_minor = N;` in api_connection.cpp"
    )
    minor = int(match.group(1))
    assert minor >= 15, (
        f"api_version_minor is {minor}, but device_capabilities requires "
        "clients to see api_version >= 1.15 in HelloResponse before they will "
        "ever request it."
    )
