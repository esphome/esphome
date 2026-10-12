"""Tests for the LVGL line widget point schema and code generation."""

from __future__ import annotations

import re

import pytest

from esphome.components.lvgl.schemas import point_schema
from esphome.config_validation import Invalid
from esphome.const import CONF_X, CONF_Y

# ---------------------------------------------------------------------------
# Validation: point_schema normalises dict / list / string to same result
# ---------------------------------------------------------------------------


class TestPointSchemaValidation:
    """Test that all point input formats normalise to the same dict."""

    @pytest.mark.parametrize(
        "dict_input,list_input,string_input",
        [
            ({CONF_X: 10, CONF_Y: 20}, [10, 20], "10, 20"),
            ({CONF_X: 0, CONF_Y: 0}, [0, 0], "0, 0"),
            ({CONF_X: 100, CONF_Y: 200}, [100, 200], "100, 200"),
            ({CONF_X: -5, CONF_Y: -10}, [-5, -10], "-5, -10"),
        ],
    )
    def test_integer_formats_produce_same_result(
        self, dict_input, list_input, string_input
    ):
        result_dict = point_schema(dict_input)
        result_list = point_schema(list_input)
        result_string = point_schema(string_input)

        assert result_dict == result_list
        assert result_dict == result_string

    def test_percentage_formats_produce_same_result(self):
        result_dict = point_schema({CONF_X: "50%", CONF_Y: "75%"})
        result_list = point_schema(["50%", "75%"])
        result_string = point_schema("50%, 75%")

        assert result_dict == result_list
        assert result_dict == result_string

    def test_pixel_suffix_matches_plain_integer(self):
        result_px = point_schema({CONF_X: "10px", CONF_Y: "20px"})
        result_int = point_schema({CONF_X: 10, CONF_Y: 20})

        assert result_px == result_int

    @pytest.mark.parametrize(
        "value",
        [
            {CONF_X: 50, CONF_Y: 75},
            [50, 75],
            "50, 75",
        ],
    )
    def test_output_contains_x_and_y(self, value):
        result = point_schema(value)

        assert CONF_X in result
        assert CONF_Y in result

    def test_list_wrong_length_raises(self):
        with pytest.raises(Invalid, match="Invalid point"):
            point_schema([1])

        with pytest.raises(Invalid, match="Invalid point"):
            point_schema([1, 2, 3])

    def test_string_without_comma_raises(self):
        with pytest.raises(Invalid, match="Invalid point"):
            point_schema("garbage")

    def test_string_extra_commas_raises(self):
        with pytest.raises(Invalid, match="Invalid point"):
            point_schema("1,2,3")


# ---------------------------------------------------------------------------
# Code generation: different point formats produce identical C++ output
# ---------------------------------------------------------------------------

_SET_POINTS_RE = re.compile(r"(\w+)->set_points\((.+?)\);")


def _extract_set_points(main_cpp: str) -> dict[str, str]:
    """Return {var_name: args_text} for every set_points() call found."""
    return {m.group(1): m.group(2) for m in _SET_POINTS_RE.finditer(main_cpp)}


class TestLineCodeGeneration:
    """Verify that alternative point formats generate identical C++ code."""

    @pytest.fixture()
    def main_cpp(self, generate_main, component_config_path) -> str:
        return generate_main(component_config_path("line_points.yaml"))

    @pytest.fixture()
    def set_points_calls(self, main_cpp) -> dict[str, str]:
        return _extract_set_points(main_cpp)

    def test_integer_points_all_formats_match(self, set_points_calls):
        """Dict, list, and string formats with integer points produce same set_points call."""
        assert set_points_calls["line_dict"] == set_points_calls["line_list"]
        assert set_points_calls["line_dict"] == set_points_calls["line_string"]

    def test_percentage_points_all_formats_match(self, set_points_calls):
        """Dict, list, and string formats with percentage points produce same set_points call."""
        assert set_points_calls["line_pct_dict"] == set_points_calls["line_pct_list"]
        assert set_points_calls["line_pct_dict"] == set_points_calls["line_pct_string"]

    def test_mixed_points_formats_match(self, set_points_calls):
        """Dict and list formats with mixed int/percent points produce same set_points call."""
        assert (
            set_points_calls["line_mixed_dict"] == set_points_calls["line_mixed_list"]
        )

    def test_integer_points_contain_expected_values(self, set_points_calls):
        """Integer points appear literally in the generated code."""
        args = set_points_calls["line_dict"]
        for val in ("10", "20", "100", "200"):
            assert val in args

    def test_percentage_points_use_lv_pct(self, set_points_calls):
        """Percentage points are generated using the lv_pct() macro."""
        args = set_points_calls["line_pct_dict"]
        assert "lv_pct(50)" in args
        assert "lv_pct(75)" in args

    def test_all_lines_present(self, set_points_calls):
        """All expected line IDs have a set_points call."""
        expected = {
            "line_dict",
            "line_list",
            "line_string",
            "line_pct_dict",
            "line_pct_list",
            "line_pct_string",
            "line_mixed_dict",
            "line_mixed_list",
        }
        assert expected.issubset(set_points_calls.keys())
