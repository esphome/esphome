"""Tests for the LVGL chart widget and series update action."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.lvgl.widgets.chart import (
    CHART_MODIFY_SCHEMA,
    CHART_SCHEMA,
    POINT_LIMIT,
    SERIES_MODIFY_SCHEMA,
    chart_spec,
    scale_value,
    series_axes,
    series_multipliers,
    validate_series_update_for_type,
)
from esphome.config import load_config
from esphome.config_validation import Invalid
from esphome.core import CORE, Lambda

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


def _validation_errors(tmp_path: Path, update: str, extra: str = "") -> list[Invalid]:
    """Validate a config with the given series update action and return the errors."""
    config_path = tmp_path / "chart.yaml"
    config_path.write_text(CHART_CONFIG_TEMPLATE.format(update=update, extra=extra))
    original_path = CORE.config_path
    try:
        CORE.config_path = config_path
        return list(load_config({}).errors)
    finally:
        CORE.config_path = original_path
        CORE.reset()


def _config_errors(tmp_path: Path, update: str, extra: str = "") -> list[str]:
    """As _validation_errors(), with the errors as text."""
    return [str(err) for err in _validation_errors(tmp_path, update, extra)]


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


@pytest.mark.parametrize("axis", ["x_axis", "y_axis"])
def test_grid_lines_are_set_on_the_primary_axes(axis: str) -> None:
    assert CHART_SCHEMA({axis: {"grid_lines": 0}})[axis]["grid_lines"] == 0
    update = CHART_MODIFY_SCHEMA({axis: {"grid_lines": 4}})
    assert update[axis] == {"grid_lines": 4}


@pytest.mark.parametrize("axis", ["secondary_x_axis", "secondary_y_axis"])
def test_secondary_axes_have_no_grid_lines(axis: str) -> None:
    with pytest.raises(Invalid):
        CHART_SCHEMA({axis: {"grid_lines": 4}})
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({axis: {"min_value": 0, "max_value": 1, "grid_lines": 4}})


@pytest.mark.parametrize(
    "axis",
    [{}, {"grid_lines": 256}, {"min_value": 0}, {"max_value": 1, "grid_lines": 2}],
)
def test_invalid_axis_updates(axis: dict) -> None:
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA({"y_axis": axis})


def test_div_line_count_is_gone() -> None:
    with pytest.raises(Invalid):
        CHART_SCHEMA({"div_line_count": [1, 2]})


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
        "lv_chart_set_next_value2(::scatter_chart, ::scatter_series, "
        "lvgl::chart_point_value(static_cast<float>(50), 1.0f), "
        "lvgl::chart_point_value(static_cast<float>(60), 1.0f));" in main_cpp
    )
    assert "lv_chart_set_next_value(line_chart, line_series, 10);" in main_cpp
    assert (
        "lv_chart_set_next_value(::line_chart, ::line_series, "
        "lvgl::chart_point_value(static_cast<float>(30), 1.0f));" in main_cpp
    )


def test_series_values_update_clears_then_appends(
    generate_main, component_config_path
) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    clear = "lv_chart_set_all_values(::line_chart, ::line_series, LV_CHART_POINT_NONE);"
    assert clear in main_cpp
    after_clear = main_cpp[main_cpp.index(clear) :]
    first = after_clear.index(
        "lv_chart_set_next_value(::line_chart, ::line_series, 1);"
    )
    second = after_clear.index(
        "lv_chart_set_next_value(::line_chart, ::line_series, 2);"
    )
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


@pytest.mark.parametrize(
    "schema,config",
    [
        (CHART_SCHEMA, {"type": Lambda("return LV_CHART_TYPE_BAR;")}),
        (CHART_SCHEMA, {"update_mode": Lambda("return LV_CHART_UPDATE_MODE_SHIFT;")}),
        (
            CHART_MODIFY_SCHEMA,
            {"update_mode": Lambda("return LV_CHART_UPDATE_MODE_CIRCULAR;")},
        ),
    ],
)
def test_chart_enums_cannot_be_lambdas(schema: Callable, config: dict) -> None:
    with pytest.raises(Invalid, match="cannot be a lambda"):
        schema(config)


@pytest.mark.parametrize("schema", [CHART_SCHEMA, CHART_MODIFY_SCHEMA])
def test_axis_range_cannot_be_zero_width(schema: Callable) -> None:
    with pytest.raises(Invalid, match="must not be equal"):
        schema({"y_axis": {"min_value": 5, "max_value": 5}})


def test_templated_axis_range_is_guarded(generate_main, component_config_path) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert (
        "lv_chart_set_axis_range(bar_chart, LV_CHART_AXIS_PRIMARY_Y, axis_min_VAR_, "
        "axis_max_VAR_ == axis_min_VAR_ ? axis_min_VAR_ + 1 : axis_max_VAR_);"
        in main_cpp
    )


def test_refresh_does_not_add_the_series_again(
    generate_main, component_config_path
) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert main_cpp.count("x = lv_chart_add_series(bar_chart,") == 1
    # Set at creation, by the refresh (the templated range only) and by lvgl.chart.update;
    # the refresh must not undo the update_mode and point_count the update sets
    assert main_cpp.count("lv_chart_set_axis_range(bar_chart,") == 3
    assert main_cpp.count("lv_chart_set_type(bar_chart,") == 1
    for option in ("update_mode", "point_count"):
        assert main_cpp.count(f"lv_chart_set_{option}(bar_chart,") == 2


def test_chart_update_sets_its_options(generate_main, component_config_path) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert (
        "lv_chart_set_update_mode(bar_chart, LV_CHART_UPDATE_MODE_CIRCULAR);"
        in main_cpp
    )
    assert "lv_chart_set_point_count(bar_chart, 5);" in main_cpp
    assert (
        "lv_chart_set_axis_range(bar_chart, LV_CHART_AXIS_PRIMARY_Y, -10, 10);"
        in main_cpp
    )


def test_series_named_like_a_trigger_argument(
    generate_main, component_config_path
) -> None:
    """The sensor's `float x` argument must not shadow a series with the ID x."""
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert "lv_chart_set_next_value(::bar_chart, ::x," in main_cpp


def test_chart_parts() -> None:
    assert "indicator" in chart_spec.parts
    # LVGL 9 has no ticks part
    assert "ticks" not in chart_spec.parts
    with pytest.raises(Invalid):
        CHART_SCHEMA({"ticks": {"line_width": 2}})


def test_series_update_errors_point_at_each_action(tmp_path: Path) -> None:
    errors = _validation_errors(
        tmp_path,
        "              id: line_series\n              point: {x: 1, y: 2}",
        "          - lvgl.chart.series.update:\n"
        "              id: line_series\n"
        "              values: [1, 2, 3]",
    )
    action = ["lvgl", 0, "widgets", 2, "button", "on_click", 0, "then"]
    assert sorted((err.path for err in errors), key=str) == [
        [*action, 0, "lvgl.chart.series.update", "point"],
        [*action, 1, "lvgl.chart.series.update", "values"],
    ]


@pytest.mark.parametrize(
    "value,multiplier,expected",
    [
        (1.25, 10, 13),
        (-2.5, 1, -3),
        (2.4, 1, 2),
        (0.5, 100, 50),
        (1e12, 1, POINT_LIMIT),
        (-1e12, 1, -POINT_LIMIT),
        (float("nan"), 10, None),
    ],
)
def test_scale_value(value: float, multiplier: float, expected: int | None) -> None:
    """Fixed values are scaled as chart_point_value() does at run time."""
    assert scale_value(value, multiplier) == expected


def test_scaled_values_and_gaps(generate_main, component_config_path) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    assert (
        "lv_chart_set_axis_range(scaled_chart, LV_CHART_AXIS_PRIMARY_Y, -5, 15);"
        in main_cpp
    )
    for value in ("13", "LV_CHART_POINT_NONE", "-25"):
        assert (
            f"lv_chart_set_next_value(scaled_chart, scaled_series, {value});"
            in main_cpp
        )
    assert (
        "lv_chart_set_next_value(::scaled_chart, ::scaled_series, "
        "lvgl::chart_point_value(static_cast<float>(0.5f), 10.0f));" in main_cpp
    )


def test_series_multipliers_follow_the_series_axes() -> None:
    config = CHART_SCHEMA(
        {
            "type": "scatter",
            "x_axis": {"multiplier": 2},
            "secondary_x_axis": {"multiplier": 3},
            "secondary_y_axis": {"multiplier": 5},
            "series": [
                {"id": "a"},
                {"id": "b", "use_secondary_x_axis": True, "use_secondary_y_axis": True},
            ],
        }
    )
    assert series_multipliers(config, config["series"][0]) == (2, 1)
    assert series_multipliers(config, config["series"][1]) == (3, 5)


@pytest.mark.parametrize(
    "axis",
    [
        {"min_value": 0.1, "max_value": 0.2},
        {"min_value": 1, "max_value": 1.4, "multiplier": 1},
        {"min_value": float("nan"), "max_value": 1},
        {"min_value": 0, "max_value": 1, "multiplier": 0},
        {"min_value": 0, "max_value": 1, "multiplier": -1},
    ],
)
def test_invalid_axis_ranges(axis: dict) -> None:
    with pytest.raises(Invalid):
        CHART_SCHEMA({"y_axis": axis})


def test_multiplier_makes_a_narrow_range_valid() -> None:
    config = CHART_SCHEMA(
        {"y_axis": {"min_value": 0.1, "max_value": 0.2, "multiplier": 100}}
    )
    assert config["y_axis"]["multiplier"] == 100


def test_multiplier_cannot_be_changed_by_an_update() -> None:
    with pytest.raises(Invalid):
        CHART_MODIFY_SCHEMA(
            {"y_axis": {"min_value": 0, "max_value": 1, "multiplier": 10}}
        )


def test_updated_range_is_checked_with_the_chart_multiplier(tmp_path: Path) -> None:
    errors = _validation_errors(
        tmp_path,
        "              id: line_series\n              value: 1",
        "          - lvgl.chart.update:\n"
        "              id: line_chart\n"
        "              y_axis:\n"
        "                min_value: 0.1\n"
        "                max_value: 0.2",
    )
    assert [str(err.path[-1]) for err in errors] == ["y_axis"]
    assert "must not be equal after scaling by multiplier (1)" in str(errors[0])


def test_grid_lines_codegen(generate_main, component_config_path) -> None:
    main_cpp = generate_main(component_config_path("chart_test.yaml"))

    # Created with no vertical and 4 horizontal lines; the update sets 2 vertical
    # lines without touching the X range
    assert "lv_chart_set_ver_div_line_count(scaled_chart, 0);" in main_cpp
    assert "lv_chart_set_hor_div_line_count(scaled_chart, 4);" in main_cpp
    assert "lv_chart_set_ver_div_line_count(scaled_chart, 2);" in main_cpp
    # Only creation sets the X range
    assert (
        main_cpp.count("lv_chart_set_axis_range(scaled_chart, LV_CHART_AXIS_PRIMARY_X")
        == 1
    )
