"""Tests for ESP8266 component."""

import pytest

from esphome.components.esp8266 import lambdas_use_scanf_float
from esphome.core import Lambda
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("src", "expected"),
    [
        # Basic float formats
        ('sscanf(buf, "%f", &v)', True),
        ('sscanf(buf, "%F", &v)', True),
        ('sscanf(buf, "%e", &v)', True),
        ('sscanf(buf, "%E", &v)', True),
        ('sscanf(buf, "%g", &v)', True),
        ('sscanf(buf, "%G", &v)', True),
        ('sscanf(buf, "%a", &v)', True),
        ('sscanf(buf, "%A", &v)', True),
        # With modifiers
        ('sscanf(buf, "%lf", &v)', True),
        ('sscanf(buf, "%Lf", &v)', True),
        ('sscanf(buf, "%8lf", &v)', True),
        ('sscanf(buf, "%*f")', True),
        ('sscanf(buf, "%.2f", &v)', True),
        # Mixed formats
        ('sscanf(buf, "%d,%f", &a, &b)', True),
        # fscanf and std::sscanf
        ('fscanf(fp, "%f", &v)', True),
        ('std::sscanf(buf, "%f", &v)', True),
        # Multi-line
        ('sscanf(buf,\n"%f", &v)', True),
        # No float format
        ('sscanf(buf, "%d", &v)', False),
        ('sscanf(buf, "%s", s)', False),
        # printf not scanf
        ('printf("%f", val)', False),
        # %f in a different statement after scanf
        ('sscanf(buf, "%d", &x); printf("%f", val);', False),
        # scanf %f in comment only
        ('// sscanf(buf, "%f", &v)\nsscanf(buf, "%d", &x)', False),
        ('/* sscanf(buf, "%f") */\nsscanf(buf, "%d", &x)', False),
    ],
)
def test_lambdas_use_scanf_float(src: str, expected: bool) -> None:
    """Test scanf float detection in lambda source."""
    config: ConfigType = {"test": [Lambda(src)]}
    assert lambdas_use_scanf_float(config) is expected


def test_lambdas_use_scanf_float_no_lambdas() -> None:
    """Test with config containing no lambdas."""
    config: ConfigType = {"key": "value", "list": [1, 2]}
    assert lambdas_use_scanf_float(config) is False


def test_lambdas_use_scanf_float_nested() -> None:
    """Test detection in deeply nested config."""
    config: ConfigType = {"a": {"b": {"c": [Lambda('sscanf(buf, "%f", &v)')]}}}
    assert lambdas_use_scanf_float(config) is True
