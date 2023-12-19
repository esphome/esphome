from __future__ import annotations
import pytest
from esphome.components import network


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
    assert actual == expected
