from __future__ import annotations

from typing import Any

import pytest

from esphome.components import network
from esphome.cpp_generator import MockObj


def _convert_mockobjs_to_string_literal(obj: Any) -> Any:
    """Convert mock objects to their base objects."""
    if isinstance(obj, MockObj):
        return str(obj.base.args.args[0].string)
    if isinstance(obj, list):
        return [_convert_mockobjs_to_string_literal(x) for x in obj]
    if isinstance(obj, tuple):
        return tuple(_convert_mockobjs_to_string_literal(x) for x in obj)
    if isinstance(obj, dict):
        return {k: _convert_mockobjs_to_string_literal(v) for k, v in obj.items()}
    return obj


@pytest.mark.parametrize(
    ("contents", "expected"),
    (
        ("127.0.0.1 localhost", [("localhost", "127.0.0.1")]),
        (":1 localhost", [("localhost", ":1")]),
        ("#192.168.1.1 commented", []),
        (
            "192.168.1.10 multi1.example.com multi1\n:1 localhost",
            [
                ("multi1.example.com", "192.168.1.10"),
                ("multi1", "192.168.1.10"),
                ("localhost", ":1"),
            ],
        ),
    ),
)
def test_parse_hosts_file(contents: str, expected: list[tuple[str, str]]):
    """Test parsing a hosts file."""
    actual = network.parse_hosts_file(contents)
    unmocked_actual = _convert_mockobjs_to_string_literal(actual)
    assert _convert_mockobjs_to_string_literal(unmocked_actual) == expected
