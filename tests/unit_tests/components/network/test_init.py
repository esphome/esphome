import pytest
from esphome.components import network


@pytest.mark.parametrize(
    "line, expected",
    (
        ("127.0.0.1 localhost", [("localhost", "127.0.0.1")]),
        (":1 localhost", [("localhost", ":1")]),
        ("#192.168.1.1 commented", []),
    ),
)
def test_parse_hosts(line, expected):
    actual = network.parse_hosts_line(line)
    assert actual == expected
