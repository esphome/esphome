import pytest
import string

from hypothesis import given
from hypothesis.strategies import text

from esphome import config_validation
from esphome.config_validation import Invalid


@given(value=text(alphabet=string.ascii_letters + string.digits))
def test_alphanumeric__valid(value):
    config_validation.alphanumeric(value)


@pytest.mark.parametrize("value", (
    "foo_bar", "foobar", "foo1"
))
def test_valid_name__valid(value):
    config_validation.valid_name(value)


@pytest.mark.parametrize("value", (
    "foo bar", "FooBar", "foo::bar"
))
def test_valid_name__invalid(value):
    with pytest.raises(Invalid):
        config_validation.valid_name(value)
