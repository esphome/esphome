"""Tests for lvgl chart widget validation (esphome/components/lvgl/widgets/chart.py)."""

from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.components.lvgl.defines import (
    CONF_ITEMS,
    CONF_PERSIST,
    CONF_POINT_COUNT,
    CONF_SERIES,
)
from esphome.components.lvgl.widgets.chart import (
    TYPE_SCATTER,
    lv_chart_t,
    validate_chart,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from esphome.core import CORE, ID

BASE_CONFIG = """
esphome:
  name: test

host:

display:
  - platform: sdl
    id: sdl0
    dimensions:
      width: 320
      height: 240

sensor:
  - platform: template
    id: my_sensor
    lambda: return 42.0;

lvgl:
  displays: sdl0
  widgets:
    - chart:
        id: my_chart
        point_count: 10
{chart_config}
"""


def _read_config(tmp_path, yaml_content: str):
    """Write `yaml_content` to a temp file and run it through the real config pipeline."""
    test_file = tmp_path / "test.yaml"
    test_file.write_text(yaml_content)

    parsed_yaml = yaml_util.load_yaml(test_file)

    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", test_file),
        # sdl's own config validation shells out to the real `sdl2-config` binary to get
        # compiler flags -- fine on a dev machine, but not installed on every CI runner. These
        # tests only need the display platform to validate as a placeholder for lvgl's required
        # `displays:` key; the actual SDL flags are never used since nothing here compiles.
        patch(
            "esphome.components.sdl.display.subprocess.check_output",
            return_value=b"",
        ),
    ):
        return config.read_config({})


def test_chart_minimal_config_is_valid(tmp_path):
    """A single-series chart with no optional axis/persist config should validate."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is not None


def test_chart_min_value_without_max_value_fails(tmp_path, capsys):
    """min_value and max_value must be set together."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        min_value: 0\n        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "If either min_value or max_value is set, both must be set" in captured.out


def test_chart_max_value_without_min_value_fails(tmp_path, capsys):
    """max_value and min_value must be set together."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        max_value: 100\n        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "If either min_value or max_value is set, both must be set" in captured.out


def test_chart_persist_requires_esp32(tmp_path, capsys):
    """persist: true is only valid on ESP32 (NVS preference blobs don't fit ESP8266/host)."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        persist: true\n        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "only available on" in captured.out


def test_chart_point_count_too_large_fails(tmp_path, capsys):
    """point_count must fit LvChartType's uint16_t N_POINTS template parameter."""
    yaml_content = BASE_CONFIG.replace("point_count: 10", "point_count: 70000").format(
        chart_config="        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "value must be at most 65535" in captured.out


def test_chart_series_list_empty_fails(tmp_path, capsys):
    """At least one series is required."""
    yaml_content = BASE_CONFIG.format(chart_config="        series: []\n")

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "length of value must be at least 1" in captured.out


def test_chart_series_list_too_long_fails(tmp_path, capsys):
    """The series list must fit LvChartType's uint8_t N_SERIES template parameter."""
    sensors = "\n".join(
        f"  - platform: template\n    id: sensor_{i}\n    lambda: return 42.0;"
        for i in range(256)
    )
    series = "\n".join(f"          - sensor: sensor_{i}" for i in range(256))
    yaml_content = f"""
esphome:
  name: test

host:

display:
  - platform: sdl
    id: sdl0
    dimensions:
      width: 320
      height: 240

sensor:
{sensors}

lvgl:
  displays: sdl0
  widgets:
    - chart:
        id: my_chart
        point_count: 10
        series:
{series}
"""

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "length of value must be at most 255" in captured.out


def test_chart_scatter_series_with_sensor_fails(tmp_path, capsys):
    """A scatter series has no fixed X value, so it can't be fed by a sensor."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        type: SCATTER\n        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "cannot be fed by a sensor" in captured.out


def test_chart_non_scatter_series_without_sensor_fails(tmp_path, capsys):
    """Every series on a non-scatter chart must have a sensor."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        series:\n          - color: 0xFF0000\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "'sensor' is required for every series" in captured.out


def test_chart_scatter_with_update_interval_fails(tmp_path, capsys):
    """update_interval has no effect on a scatter chart; points come from lvgl.chart.add_point."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        type: SCATTER\n        update_interval: 5s\n"
        "        series:\n          - color: 0xFF0000\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "'update_interval' has no effect on a scatter chart" in captured.out


def test_chart_point_radius_on_unsupported_type_fails(tmp_path, capsys):
    """point_radius only applies to LINE and SCATTER charts."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        type: BAR\n        point_radius: 3px\n"
        "        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "'point_radius' has no effect on a 'LV_CHART_TYPE_BAR' chart" in captured.out


def test_chart_stacked_negative_min_value_fails(tmp_path, capsys):
    """A stacked chart only supports positive values."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        type: STACKED\n        min_value: -5\n        max_value: 100\n"
        "        series:\n          - sensor: my_sensor\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is None
    captured = capsys.readouterr()
    assert "A stacked chart only supports positive values" in captured.out


def test_chart_scatter_valid_config_defaults_line_width_to_zero(tmp_path):
    """A scatter chart with no explicit `items:` gets line_width: 0 (a point cloud, not connected
    lines), since LVGL always draws a connecting line between scatter points otherwise."""
    yaml_content = BASE_CONFIG.format(
        chart_config="        type: SCATTER\n"
        "        series:\n          - color: 0xFF0000\n          - color: 0x0000FF\n"
    )

    result = _read_config(tmp_path, yaml_content)

    assert result is not None
    chart_config = result["lvgl"][0]["widgets"][0]["chart"]
    assert chart_config[CONF_ITEMS] == {"line_width": 0}


def _base_validate_chart_config(**overrides) -> dict:
    """A minimal, already-schema-shaped config dict for calling validate_chart() directly.

    Used for branches that are awkward to reach through a full YAML config (e.g. persist's
    ESP32-only check would fire before validate_chart's own scatter+persist check, since it runs
    off a separate schema key's validator) -- calling validate_chart() directly isolates the exact
    branch under test from unrelated schema-level checks.
    """
    config = {
        CONF_ID: ID("my_chart", is_declaration=True, type=lv_chart_t),
        CONF_TYPE: "LV_CHART_TYPE_LINE",
        CONF_POINT_COUNT: 10,
        CONF_PERSIST: False,
        CONF_SERIES: [{"color": 0, "sensor": ID("my_sensor")}],
    }
    config.update(overrides)
    return config


def test_validate_chart_scatter_with_persist_fails():
    """Persist isn't supported on a scatter chart: its stored history has no room for X values."""
    config = _base_validate_chart_config(
        **{
            CONF_TYPE: TYPE_SCATTER,
            CONF_PERSIST: True,
            CONF_SERIES: [{"color": 0}],
        }
    )

    with pytest.raises(cv.Invalid, match="not supported on a scatter chart"):
        validate_chart(config)
