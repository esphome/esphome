import pytest

from hypothesis import given
from hypothesis import strategies as st
from hypothesis.provisional import ip4_addr_strings
from strategies import mac_addr_strings

from esphome import core


class TestHexInt:
    @pytest.mark.parametrize("value, expected", (
            (1, "0x01"),
            (255, "0xFF"),
            (128, "0x80"),
            (256, "0x100"),
            (-1, "-0x01"),  # TODO: this currently fails
    ))
    def test_str(self, value, expected):
        target = core.HexInt(value)

        actual = str(target)

        assert actual == expected


class TestIPAddress:
    @given(value=ip4_addr_strings())
    def test_init__valid(self, value):
        core.IPAddress(*value.split("."))

    @pytest.mark.parametrize("value", ("127.0.0", "localhost", ""))
    def test_init__invalid(self, value):
        with pytest.raises(ValueError, match="IPAddress must consist of 4 items"):
            core.IPAddress(*value.split("."))

    @given(value=ip4_addr_strings())
    def test_str(self, value):
        target = core.IPAddress(*value.split("."))

        actual = str(target)

        assert actual == value


class TestMACAddress:
    @given(value=mac_addr_strings())
    def test_init__valid(self, value):
        core.MACAddress(*value.split(":"))

    @pytest.mark.parametrize("value", ("1:2:3:4:5", "localhost", ""))
    def test_init__invalid(self, value):
        with pytest.raises(ValueError, match="MAC Address must consist of 6 items"):
            core.MACAddress(*value.split(":"))

    @given(value=mac_addr_strings())
    def test_str(self, value):
        target = core.MACAddress(*(int(v, 16) for v in value.split(":")))

        actual = str(target)

        assert actual == value

    def test_as_hex(self):
        target = core.MACAddress(0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF)

        actual = target.as_hex

        assert actual.text == "0xDEADBEEF00FFULL"


@pytest.mark.parametrize("value", (
    1, 2, -1, 0, 1.0, -1.0, 42.0009, -42.0009
))
def test_is_approximately_integer__in_range(value):
    actual = core.is_approximately_integer(value)

    assert actual is True


@pytest.mark.parametrize("value", (
    42.01, -42.01, 1.5
))
def test_is_approximately_integer__not_in_range(value):
    actual = core.is_approximately_integer(value)

    assert actual is False


class TestTimePeriod:
    @pytest.mark.parametrize("kwargs, expected", (
            ({}, "0s"),
            ({"microseconds": 1}, "1us"),
            ({"microseconds": 1.0001}, "1us"),
            ({"milliseconds": 2}, "2ms"),
            ({"milliseconds": 2.0001}, "2ms"),
            ({"milliseconds": 2.01}, "2010us"),
            ({"seconds": 3}, "3s"),
            ({"seconds": 3.0001}, "3s"),
            ({"seconds": 3.01}, "3010ms"),
            ({"minutes": 4}, "4min"),
            ({"minutes": 4.0001}, "4min"),
            ({"minutes": 4.1}, "246s"),
            ({"hours": 5}, "5h"),
            ({"hours": 5.0001}, "5h"),
            ({"hours": 5.1}, "306min"),
            ({"days": 6}, "6d"),
            ({"days": 6.0001}, "6d"),
            ({"days": 6.1}, "8784min"),
    ))
    def test_init(self, kwargs, expected):
        target = core.TimePeriod(**kwargs)

        actual = str(target)

        assert actual == expected
