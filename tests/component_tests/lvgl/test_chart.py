"""Tests for the LVGL chart widget and series update action."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome.components.lvgl.widgets.chart import (
    CHART_MODIFY_SCHEMA,
    CHART_SCHEMA,
    SERIES_MODIFY_SCHEMA,
    series_axes,
    validate_series_update_for_type,
)
from esphome.config import load_config
from esphome.config_validation import Invalid
from esphome.core import CORE

CHART_CONFIG_TEMPLATE = """
esphome:
  name: test-chart

esp32:
  board: lolin_c3_mini

spi:
  mosi_pin:
    number: GPIO2
    ignore_strapping_warning: true
  clk_pin: GPIO1

display:
  - platform: mipi_spi
    data_rate: 20MHz
    model: st7735
    cs_pin:
      number: GPIO8
      ignore_strapping_warning: true
    dc_pin: GPIO3

lvgl:
  widgets:
    - chart:
        id: line_chart
        point_count: 2
        series:
          - id: line_series
    - chart:
        id: scatter_chart
        type: scatter
        point_count: 2
        series:
          - id: scatter_series
    - button:
        on_click:
          - lvgl.chart.series.update:
{update}
{extra}
"""


def _config_errors(tmp_path: Path, update: str, extra: str = "") -> list[str]:
    """Validate a config with the given series update action and return the errors as text."""
    config_path = tmp_path / "chart.yaml"
    config_path.write_text(CHART_CONFIG_TEMPLATE.format(update=update, extra=extra))
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        return [str(err) for err in load_config({}).errors]
    finally:
        CORE.config_path = original_path
        CORE.reset()


@pytest.mark.parametrize(
    "config",
    [
        {"id": "series"},
        {"id": "series", "value": 1, "values": [2]},
        {"id": "series", "value": 1, "point": {"x": 10, "y": 20}},
    ],
)
def test_series_update_requires_exactly_one_payload(config: dict) -> None:
    with pytest.raises(Invalid):
        SERIES_MODIFY_SCHEMA(config)


@pytest.mark.parametrize(
    "key,payload",
    [("value", 1), ("values", [1, 2]), ("point", {"x": 10, "y": 20})],
)
def test_series_update_accepts_one_payload(key: str, payload: object) -> None:
    assert SERIES_MODIFY_SCHEMA({"id": "series", key: payload})[key] == payload


@pytest.mark.parametrize("div_line_count", [[1], [1, 2, 3]])
def test_div_line_count_needs_two_values(div_line_count: list[int]) -> None:
    with pytest.raises(Invalid):
        CHART_SCHEMA({"div_line_count": div_line_count})
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({"div_line_count": div_line_count})


def test_chart_update_rejects_type_change() -> None:
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({"type": "scatter"})


@pytest.mark.parametrize(
    "chart_type,payload",
    [
        ("LV_CHART_TYPE_SCATTER", {"value": 1}),
        ("LV_CHART_TYPE_SCATTER", {"values": [1, 2]}),
        ("LV_CHART_TYPE_LINE", {"point": {"x": 1, "y": 2}}),
        ("LV_CHART_TYPE_BAR", {"point": {"x": 1, "y": 2}}),
    ],
)
def test_series_update_payload_must_match_chart_type(
    chart_type: str, payload: dict
) -> None:
    with pytest.raises(Invalid):
        validate_series_update_for_type(payload, chart_type, 10)


def test_scatter_rejects_values() -> None:
    with pytest.raises(Invalid, match="scatter"):
        CHART_SCHEMA({"type": "scatter", "series": [{"id": "s", "values": [1]}]})


def test_line_rejects_points() -> None:
    with pytest.raises(Invalid, match="points"):
        CHART_SCHEMA(
            {
                "type": "line",
                "series": [{"id": "s", "points": [{"x": 1, "y": 2}]}],
            }
        )


def test_scatter_point_requires_x_and_y() -> None:
    with pytest.raises(Invalid):
        CHART_SCHEMA({"type": "scatter", "series": [{"id": "s", "points": [{"x": 1}]}]})


def test_scatter_points_cannot_exceed_point_count() -> None:
    with pytest.raises(Invalid, match="point_count"):
        CHART_SCHEMA(
            {
                "type": "scatter",
                "point_count": 1,
                "series": [
                    {
                        "id": "s",
                        "points": [{"x": 1, "y": 2}, {"x": 3, "y": 4}],
                    }
                ],
            }
        )


def test_scatter_codegen_uses_paired_lvgl_apis(
    generate_main, component_config_path
) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert (
        "lv_chart_set_next_value2(scatter_chart, scatter_series, 10, 20);" in main_cpp
    )
    assert (
        "lv_chart_set_next_value2(scatter_chart, scatter_series, 30, 40);" in main_cpp
    )
    assert (
        "lv_chart_set_next_value2(scatter_chart, scatter_series, "
        "static_cast<int>(50), static_cast<int>(60));" in main_cpp
    )
    assert "lv_chart_set_next_value(line_chart, line_series, 10);" in main_cpp
    assert (
        "lv_chart_set_next_value(line_chart, line_series, static_cast<int>(30));"
        in main_cpp
    )


def test_series_values_update_clears_then_appends(
    generate_main, component_config_path
) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    clear = "lv_chart_set_all_values(line_chart, line_series, LV_CHART_POINT_NONE);"
    assert clear in main_cpp
    after_clear = main_cpp[main_cpp.index(clear) :]
    first = after_clear.index("lv_chart_set_next_value(line_chart, line_series, 1);")
    second = after_clear.index("lv_chart_set_next_value(line_chart, line_series, 2);")
    assert first < second


@pytest.mark.parametrize(
    "axis",
    ["x_axis", "y_axis", "secondary_x_axis", "secondary_y_axis"],
)
def test_chart_update_requires_complete_axis_range(axis: str) -> None:
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({axis: {"max_value": 100}})

    config = CHART_MODIFY_SCHEMA({axis: {"min_value": -50, "max_value": 100}})
    assert config[axis] == {"min_value": -50, "max_value": 100}


def test_series_update_values_cannot_exceed_point_count() -> None:
    with pytest.raises(Invalid, match="point_count"):
        validate_series_update_for_type({"values": [1, 2, 3]}, "LV_CHART_TYPE_LINE", 2)


def test_series_update_values_can_match_point_count() -> None:
    config = {"values": [1, 2]}
    assert validate_series_update_for_type(config, "LV_CHART_TYPE_LINE", 2) == config


@pytest.mark.parametrize(
    "update,message",
    [
        (
            "id: scatter_series\n              value: 1",
            "'scatter_series': A scatter chart series update requires point",
        ),
        (
            "id: line_series\n              point: {x: 1, y: 2}",
            "'line_series': point can only update a scatter chart series",
        ),
        (
            "id: line_series\n              values: [1, 2, 3]",
            "can't have more than point_count (2) values",
        ),
    ],
)
def test_series_update_is_checked_against_its_chart(
    tmp_path: Path, update: str, message: str
) -> None:
    errors = _config_errors(tmp_path, f"              {update}")
    assert any(message in err for err in errors), errors


@pytest.mark.parametrize(
    "update",
    [
        "id: line_series\n              values: [1, 2]",
        "id: scatter_series\n              point: {x: 1, y: 2}",
    ],
)
def test_matching_series_update_is_accepted(tmp_path: Path, update: str) -> None:
    assert _config_errors(tmp_path, f"              {update}") == []


def test_series_update_is_checked_against_a_smaller_runtime_point_count(
    tmp_path: Path,
) -> None:
    errors = _config_errors(
        tmp_path,
        "              id: line_series\n              values: [1, 2]",
        "          - lvgl.chart.update:\n"
        "              id: line_chart\n"
        "              point_count: 1",
    )
    assert any("can't have more than point_count (1) values" in e for e in errors)


def test_larger_runtime_point_count_keeps_the_configured_limit(
    tmp_path: Path,
) -> None:
    errors = _config_errors(
        tmp_path,
        "              id: line_series\n              values: [1, 2, 3]",
        "          - lvgl.chart.update:\n"
        "              id: line_chart\n"
        "              point_count: 5",
    )
    assert any("can't have more than point_count (2) values" in e for e in errors)


BOTH_SECONDARY_AXES = "static_cast<lv_chart_axis_t>(LV_CHART_AXIS_SECONDARY_Y | LV_CHART_AXIS_SECONDARY_X)"


@pytest.mark.parametrize(
    "series,axes",
    [
        ({}, "LV_CHART_AXIS_PRIMARY_Y"),
        ({"use_secondary_y_axis": True}, "LV_CHART_AXIS_SECONDARY_Y"),
        ({"use_secondary_x_axis": True}, "LV_CHART_AXIS_SECONDARY_X"),
        (
            {"use_secondary_y_axis": True, "use_secondary_x_axis": True},
            BOTH_SECONDARY_AXES,
        ),
    ],
)
def test_series_axes(series: dict, axes: str) -> None:
    config = CHART_SCHEMA({"type": "scatter", "series": [{"id": "s", **series}]})
    assert str(series_axes(config["series"][0])) == axes


def test_secondary_x_axis_needs_a_scatter_chart() -> None:
    with pytest.raises(Invalid, match="use_secondary_x_axis"):
        CHART_SCHEMA(
            {"type": "line", "series": [{"id": "s", "use_secondary_x_axis": True}]}
        )


def test_series_without_a_color_gets_one_from_the_palette(
    generate_main, component_config_path
) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert (
        "lv_chart_add_series(line_chart, lv_palette_main(LV_PALETTE_BLUE), "
        "LV_CHART_AXIS_PRIMARY_Y)" in main_cpp
    )
    assert (
        "lv_chart_add_series(line_chart, lv_palette_main(LV_PALETTE_RED), "
        "LV_CHART_AXIS_PRIMARY_Y)" in main_cpp
    )
