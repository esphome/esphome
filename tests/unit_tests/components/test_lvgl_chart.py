"""Tests for lvgl chart widget validation (esphome/components/lvgl/widgets/chart.py)."""

from unittest.mock import patch

from esphome import config, yaml_util
from esphome.core import CORE

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
