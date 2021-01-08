import pytest
import string

from hypothesis import given, example
from hypothesis.strategies import one_of, text, integers, booleans, builds

from esphome import config_validation
from esphome.config_validation import Invalid
from esphome.core import Lambda, HexInt


def test_check_not_templatable__invalid():
    with pytest.raises(Invalid, match="This option is not templatable!"):
        config_validation.check_not_templatable(Lambda(""))


@pytest.mark.parametrize("value", ("foo", 1, "D12", False))
def test_alphanumeric__valid(value):
    actual = config_validation.alphanumeric(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", ("£23", "Foo!"))
def test_alphanumeric__invalid(value):
    with pytest.raises(Invalid):
        actual = config_validation.alphanumeric(value)


@given(value=text(alphabet=string.ascii_lowercase + string.digits + "_-"))
def test_valid_name__valid(value):
    actual = config_validation.valid_name(value)

    assert actual == value


@pytest.mark.parametrize("value", (
    "foo bar", "FooBar", "foo::bar"
))
def test_valid_name__invalid(value):
    with pytest.raises(Invalid):
        config_validation.valid_name(value)


@given(one_of(integers(), text()))
def test_string__valid(value):
    actual = config_validation.string(value)

    assert actual == str(value)


@pytest.mark.parametrize("value", (
    {}, [], True, False, None
))
def test_string__invalid(value):
    with pytest.raises(Invalid):
        config_validation.string(value)


@given(text())
def test_strict_string__valid(value):
    actual = config_validation.string_strict(value)

    assert actual == value


@pytest.mark.parametrize("value", (None, 123))
def test_string_string__invalid(value):
    with pytest.raises(Invalid, match="Must be string, got"):
        config_validation.string_strict(value)


@given(builds(lambda v: "mdi:" + v, text()))
@example("")
def test_icon__valid(value):
    actual = config_validation.icon(value)

    assert actual == value


def test_icon__invalid():
    with pytest.raises(Invalid, match="Icons should start with prefix"):
        config_validation.icon("foo")


@pytest.mark.parametrize("value", (
    "True", "YES", "on", "enAblE", True
))
def test_boolean__valid_true(value):
    assert config_validation.boolean(value) is True


@pytest.mark.parametrize("value", (
    "False", "NO", "off", "disAblE", False
))
def test_boolean__valid_false(value):
    assert config_validation.boolean(value) is False


@pytest.mark.parametrize("value", (
    None, 1, 0, "foo"
))
def test_boolean__invalid(value):
    with pytest.raises(Invalid, match="Expected boolean value"):
        config_validation.boolean(value)


# TODO: ensure_list
@given(integers())
def hex_int__valid(value):
    actual = config_validation.hex_int(value)

    assert isinstance(actual, HexInt)
    assert actual == value
