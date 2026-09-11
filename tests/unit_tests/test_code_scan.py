"""Unit tests for esphome.code_scan."""

from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.code_scan import (
    external_components_use_scanf_float,
    includes_use_scanf_float,
    keep_float_scanf,
    lambdas_use_scanf_float,
    user_code_uses_scanf_float,
)
from esphome.const import CONF_ESPHOME, CONF_INCLUDES, CONF_INCLUDES_C
from esphome.core import CORE, Lambda
from esphome.loader import CORE_COMPONENTS_PATH, FileResource
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
        # Mixed formats, including a ';' inside the format string
        ('sscanf(buf, "%d,%f", &a, &b)', True),
        ('sscanf(buf, "%d;%f", &a, &b)', True),
        ('sscanf(buf, "a;b", &a); sscanf(buf, "%d", &b); printf("%f", v)', False),
        # Escaped quotes, nested calls, a ')' inside the format, an unterminated call
        ('sscanf(buf, "\\"%f\\"", &v)', True),
        ('sscanf(f(x), "%f", &v)', True),
        ('sscanf(buf, "%d)", &a); g("%f")', False),
        ('sscanf(buf, "%d", &a; g("%f")', False),
        ('sscanf(buf, "%f"', True),
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


def test_includes_use_scanf_float(setup_core: Path) -> None:
    """A float scanf in an includes: or includes_c: file is found; comments and non-source files are ignored."""
    (setup_core / "parse.h").write_text(
        'static void f(const char *b) { float v; sscanf(b, "%f", &v); }'
    )
    (setup_core / "lib").mkdir()
    (setup_core / "lib" / "int.h").write_text(
        'static int g(const char *b) { int v; sscanf(b, "%d", &v); return v; }'
    )
    (setup_core / "lib" / "notes.txt").write_text('sscanf(b, "%f", &v)')
    (setup_core / "commented.h").write_text(
        '// sscanf(b, "%f", &v)\n/* sscanf(b, "%g", &v) */'
    )

    assert (
        includes_use_scanf_float({CONF_ESPHOME: {CONF_INCLUDES: ["parse.h"]}}) is True
    )
    assert (
        includes_use_scanf_float({CONF_ESPHOME: {CONF_INCLUDES_C: ["parse.h"]}}) is True
    )
    assert includes_use_scanf_float({CONF_ESPHOME: {CONF_INCLUDES: ["lib"]}}) is False
    assert (
        includes_use_scanf_float(
            {CONF_ESPHOME: {CONF_INCLUDES: ["commented.h", "<cstdio>"]}}
        )
        is False
    )
    assert includes_use_scanf_float({}) is False


def test_keep_float_scanf() -> None:
    """Explicit values win; unset follows whether user code scans a float."""
    CORE.config = {}
    float_config: ConfigType = {"test": [Lambda('sscanf(b, "%f", &v)')]}
    int_config: ConfigType = {"test": [Lambda('sscanf(b, "%d", &v)')]}
    assert keep_float_scanf(True, int_config, "x", "y") is True
    assert keep_float_scanf(False, float_config, "x", "y") is False
    assert keep_float_scanf(None, float_config, "x", "y") is True
    assert keep_float_scanf(None, int_config, "x", "y") is False


def test_external_components_use_scanf_float(setup_core: Path) -> None:
    """Sources of components outside the esphome package are scanned; in-tree ones are skipped."""

    def manifest(package_dir: Path, *sources: str) -> MagicMock:
        m = MagicMock()
        m.is_platform_component = False
        m.module.__file__ = str(package_dir / "__init__.py")
        m.resources = [FileResource("pkg", name) for name in sources]
        return m

    (setup_core / "ext_float").mkdir()
    (setup_core / "ext_float" / "parse.cpp").write_text(
        'void f(const char *b) { float v; sscanf(b, "%f", &v); }'
    )
    (setup_core / "ext_int").mkdir()
    (setup_core / "ext_int" / "parse.cpp").write_text(
        'int g(const char *b) { int v; sscanf(b, "%d", &v); return v; }'
    )
    lookup = {
        "ext_float": manifest(setup_core / "ext_float", "parse.cpp"),
        "ext_int": manifest(setup_core / "ext_int", "parse.cpp"),
        "sensor": manifest(CORE_COMPONENTS_PATH / "sensor", "sensor.cpp"),
        "esphome": manifest(CORE_COMPONENTS_PATH.parent / "core", "config.cpp"),
    }

    with patch("esphome.config.get_component", side_effect=lookup.get):
        CORE.config = {"esphome": {}, "sensor": {}, "ext_int": {}}
        assert external_components_use_scanf_float() is False
        CORE.config = {"sensor": {}, "ext_float": {}}
        assert external_components_use_scanf_float() is True
        assert user_code_uses_scanf_float(CORE.config) is True
