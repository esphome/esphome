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
            ({}, {}),
            ({"microseconds": 1}, {"microseconds": 1}),
            ({"microseconds": 1.0001}, {"microseconds": 1}),
            ({"milliseconds": 2}, {"milliseconds": 2}),
            ({"milliseconds": 2.0001}, {"milliseconds": 2}),
            ({"milliseconds": 2.01}, {"milliseconds": 2, "microseconds": 10}),
            ({"seconds": 3}, {"seconds": 3}),
            ({"seconds": 3.0001}, {"seconds": 3}),
            ({"seconds": 3.01}, {"seconds": 3, "milliseconds": 10}),
            ({"minutes": 4}, {"minutes": 4}),
            ({"minutes": 4.0001}, {"minutes": 4}),
            ({"minutes": 4.1}, {"minutes": 4, "seconds": 6}),
            ({"hours": 5}, {"hours": 5}),
            ({"hours": 5.0001}, {"hours": 5}),
            ({"hours": 5.1}, {"hours": 5, "minutes": 6}),
            ({"days": 6}, {"days": 6}),
            ({"days": 6.0001}, {"days": 6}),
            ({"days": 6.1}, {"days": 6, "hours": 2, "minutes": 24}),
    ))
    def test_init(self, kwargs, expected):
        target = core.TimePeriod(**kwargs)

        actual = target.as_dict()

        assert actual == expected

    def test_init__microseconds_with_fraction(self):
        with pytest.raises(ValueError, match="Maximum precision is microseconds"):
            core.TimePeriod(microseconds=1.1)

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
    def test_str(self, kwargs, expected):
        target = core.TimePeriod(**kwargs)

        actual = str(target)

        assert actual == expected

    # TODO: Confirm behaviour of comparisons
    #       - only uses microseconds (and milliseconds)
    #       - Raises ValueError, if a value can't be compared this should return
    #         NotImplemented so that Python can check any __ieq__ comparisons.


SAMPLE_LAMBDA = """
it.strftime(64, 0, id(my_font), TextAlign::TOP_CENTER, "%H:%M:%S", id(esptime).now());
it.printf(64, 16, id(my_font2), TextAlign::TOP_CENTER, "%.1f°C (%.1f%%)", id( office_tmp ).state, id(office_hmd).state);
"""


class TestLambda:
    def test_init__copy_initializer(self):
        value = core.Lambda("foo")
        target = core.Lambda(value)

        assert str(target) is value.value

    def test_parts(self):
        target = core.Lambda(SAMPLE_LAMBDA.strip())

        # Check cache
        assert target._parts is None
        actual = target.parts
        assert target._parts is actual
        assert target.parts is actual

        assert actual == [
            "it.strftime(64, 0, ",
            "my_font",
            "",
            ", TextAlign::TOP_CENTER, \"%H:%M:%S\", ",
            "esptime",
            ".",
            "now());\nit.printf(64, 16, ",
            "my_font2",
            "",
            ", TextAlign::TOP_CENTER, \"%.1f°C (%.1f%%)\", ",
            "office_tmp",
            ".",
            "state, ",
            "office_hmd",
            ".",
            "state);"
        ]

    def test_requires_ids(self):
        target = core.Lambda(SAMPLE_LAMBDA.strip())

        # Check cache
        assert target._requires_ids is None
        actual = target.requires_ids
        assert target._requires_ids is actual
        assert target.requires_ids is actual

        assert actual == [
            core.ID("my_font"),
            core.ID("esptime"),
            core.ID("my_font2"),
            core.ID("office_tmp"),
            core.ID("office_hmd"),
        ]

    def test_value_setter(self):
        target = core.Lambda("")

        # Populate cache
        _ = target.parts
        _ = target.requires_ids

        target.value = SAMPLE_LAMBDA

        # Check cache has been cleared
        assert target._parts is None
        assert target._requires_ids is None

        assert target.value == SAMPLE_LAMBDA

    def test_repr(self):
        target = core.Lambda("id(var).value == 1")

        assert repr(target) == "Lambda<id(var).value == 1>"


class TestID:
    @pytest.fixture
    def target(self):
        return core.ID(None, is_declaration=True, type="binary_sensor::Example")

    @pytest.mark.parametrize("id, is_manual, expected", (
            ("foo", None, True),
            (None, None, False),
            ("foo", True, True),
            ("foo", False, False),
            (None, True, True),
    ))
    def test_init__resolve_is_manual(self, id, is_manual, expected):
        target = core.ID(id, is_manual=is_manual)

        assert target.is_manual == expected

    @pytest.mark.parametrize("registered_ids, expected", (
            ([], "binary_sensor_example"),
            (["binary_sensor_example"], "binary_sensor_example_2"),
            (["foo"], "binary_sensor_example"),
            (["binary_sensor_example", "foo", "binary_sensor_example_2"], "binary_sensor_example_3"),
    ))
    def test_resolve(self, target, registered_ids, expected):
        actual = target.resolve(registered_ids)

        assert actual == expected
        assert str(target) == expected

    def test_copy(self, target):
        target.resolve([])

        actual = target.copy()

        assert actual is not target
        assert all(getattr(actual, n) == getattr(target, n)
                   for n in ("id", "is_declaration", "type", "is_manual"))


class TestDocumentLocation:
    @pytest.fixture
    def target(self):
        return core.DocumentLocation(
            document="foo.txt",
            line=10,
            column=20,
        )

    def test_str(self, target):
        actual = str(target)

        assert actual == "foo.txt 10:20"


class TestDocumentRange:
    @pytest.fixture
    def target(self):
        return core.DocumentRange(
            core.DocumentLocation(
                document="foo.txt",
                line=10,
                column=20,
            ),
            core.DocumentLocation(
                document="foo.txt",
                line=15,
                column=12,
            ),
        )

    def test_str(self, target):
        actual = str(target)

        assert actual == "[foo.txt 10:20 - foo.txt 15:12]"


class TestDefine:
    @pytest.mark.parametrize("name, value, prop, expected", (
            ("ANSWER", None, "as_build_flag", "-DANSWER"),
            ("ANSWER", None, "as_macro", "#define ANSWER"),
            ("ANSWER", None, "as_tuple", ("ANSWER", None)),
            ("ANSWER", 42, "as_build_flag", "-DANSWER=42"),
            ("ANSWER", 42, "as_macro", "#define ANSWER 42"),
            ("ANSWER", 42, "as_tuple", ("ANSWER", 42)),
    ))
    def test_properties(self, name, value, prop, expected):
        target = core.Define(name, value)

        actual = getattr(target, prop)

        assert actual == expected


class TestLibrary:
    @pytest.mark.parametrize("name, value, prop, expected", (
            ("mylib", None, "as_lib_dep", "mylib"),
            ("mylib", None, "as_tuple", ("mylib", None)),
            ("mylib", "1.2.3", "as_lib_dep", "mylib@1.2.3"),
            ("mylib", "1.2.3", "as_tuple", ("mylib", "1.2.3")),
    ))
    def test_properties(self, name, value, prop, expected):
        target = core.Library(name, value)

        actual = getattr(target, prop)

        assert actual == expected
