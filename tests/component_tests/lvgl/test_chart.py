"""Tests for the LVGL chart widget and series update action."""

from __future__ import annotations

import pytest

from esphome.components.lvgl.widgets.chart import (
    CHART_MODIFY_SCHEMA,
    CHART_SCHEMA,
    SERIES_MODIFY_SCHEMA,
    validate_series_update_for_type,
)
from esphome.config_validation import Invalid


@pytest.mark.parametrize(
    "config",
    [
        {"id": "series"},
        {"id": "series", "value": 1, "values": [2]},
    ],
)
def test_series_update_requires_exactly_one_payload(config: dict) -> None:
    with pytest.raises(Invalid):
        SERIES_MODIFY_SCHEMA(config)


def test_series_update_accepts_one_value() -> None:
    assert SERIES_MODIFY_SCHEMA({"id": "series", "value": 1})["value"] == 1


def test_series_update_accepts_one_point() -> None:
    config = SERIES_MODIFY_SCHEMA({"id": "series", "point": {"x": 10, "y": 20}})

    assert config["point"] == {"x": 10, "y": 20}


def test_series_update_rejects_point_with_value() -> None:
    with pytest.raises(Invalid):
        SERIES_MODIFY_SCHEMA({"id": "series", "value": 1, "point": {"x": 10, "y": 20}})


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
        "lv_chart_set_series_value_by_id2(scatter_chart, scatter_series, 0, 10, 20);"
        in main_cpp
    )
    assert (
        "lv_chart_set_series_value_by_id2(scatter_chart, scatter_series, 1, 30, 40);"
        in main_cpp
    )
    assert (
        "lv_chart_set_next_value2(scatter_chart, scatter_series, "
        "static_cast<int>(50), static_cast<int>(60));" in main_cpp
    )
    assert "lv_chart_set_value_by_id(line_chart, line_series, 0, 10);" in main_cpp
    assert (
        "lv_chart_set_next_value(line_chart, line_series, static_cast<int>(30));"
        in main_cpp
    )


@pytest.mark.parametrize(
    "axis",
    ["x_axis", "y_axis", "secondary_x_axis", "secondary_y_axis"],
)
def test_chart_update_requires_complete_axis_range(axis: str) -> None:
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({axis: {"max_value": 100}})

    config = CHART_MODIFY_SCHEMA(
        {axis: {"min_value": -50, "max_value": 100}}
    )
    assert config[axis] == {"min_value": -50, "max_value": 100}


def test_series_update_values_cannot_exceed_point_count() -> None:
    with pytest.raises(Invalid, match="point_count"):
        validate_series_update_for_type(
            {"values": [1, 2, 3]}, "LV_CHART_TYPE_LINE", 2
        )


def test_series_update_values_can_match_point_count() -> None:
    config = {"values": [1, 2]}
    assert (
        validate_series_update_for_type(config, "LV_CHART_TYPE_LINE", 2)
        == config
    )
