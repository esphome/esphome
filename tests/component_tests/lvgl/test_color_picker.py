"""Tests for the LVGL color picker widget schema and code generation."""

from __future__ import annotations

import re

import pytest

from esphome.components.lvgl.schemas import container_schema
from esphome.components.lvgl.widgets.color_picker import color_picker_spec
from esphome.config_validation import Invalid

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


class TestColorPickerValidation:
    """Test the widget's own schema options."""

    @pytest.fixture()
    def schema(self):
        return container_schema(color_picker_spec)

    @pytest.mark.parametrize("width", [100, "50%", "SIZE_CONTENT"])
    def test_width_accepted(self, schema, width):
        assert schema({"width": width}) is not None

    def test_width_is_required(self, schema):
        with pytest.raises(Invalid, match="required key not provided.*width"):
            schema({})

    def test_height_is_rejected(self, schema):
        """The widget is square, so a height of its own makes no sense."""
        with pytest.raises(Invalid, match="Height will be set to the same as width"):
            schema({"width": 100, "height": 100})

    def test_color_is_optional(self, schema):
        assert "color" not in schema({"width": 100})

    def test_on_value_is_accepted(self, schema):
        """The trigger is only offered if the widget declares it has a value."""
        result = schema({"width": 100, "on_value": [{"logger.log": "picked"}]})

        assert "on_value" in result

    def test_value_argument_is_a_color(self):
        """`x` must be an esphome Color, so that lambdas can use x.r/x.g/x.b.

        esphome::Color converts to lv_color_t implicitly, so declaring the wrong
        one of the two still compiles but gives lambdas the LVGL struct, whose
        members are named differently.
        """
        (arg_type,) = color_picker_spec.w_type.get_arg_type()

        assert str(arg_type) == "Color"

    def test_update_schema_has_no_width(self):
        """Width belongs to creation only; the update action must not require it."""
        assert "width" not in str(color_picker_spec.modify_schema.schema)


# ---------------------------------------------------------------------------
# Code generation
# ---------------------------------------------------------------------------

_SET_STYLE_RE = re.compile(r"lv_obj_set_style_(\w+)\((\w+)->obj, (.+?), \w+\);")


def _styles_for(main_cpp: str, var: str) -> dict[str, str]:
    """Return {property: value} for every style set directly on a widget."""
    return {
        m.group(1): m.group(3)
        for m in _SET_STYLE_RE.finditer(main_cpp)
        if m.group(2) == var
    }


class TestColorPickerCodeGeneration:
    @pytest.fixture()
    def main_cpp(self, generate_main, component_config_path) -> str:
        return generate_main(component_config_path("color_picker.yaml"))

    def test_height_follows_width(self, main_cpp):
        """The widget is square, so height is generated to match the given width."""
        for var, expected in (
            ("picker_content", "LV_SIZE_CONTENT"),
            ("picker_fixed", "120"),
            ("picker_colour_id", "lv_pct(50)"),
        ):
            styles = _styles_for(main_cpp, var)
            assert styles["width"] == expected
            assert styles["height"] == expected

    def test_color_is_applied(self, main_cpp):
        assert "picker_fixed->set_color(" in main_cpp
        assert "picker_colour_id->set_color(" in main_cpp

    def test_color_is_omitted_when_not_configured(self, main_cpp):
        assert "picker_content->set_color(" not in main_cpp

    def test_trigger_uses_color_type(self, main_cpp):
        """The on_value trigger is declared with the Color the widget reports."""
        assert "Trigger<Color, lv_event_t *>" in main_cpp

    def test_trigger_reads_the_current_color(self, main_cpp):
        assert "picker_colour_id->get_color()" in main_cpp

    def test_update_action_generates_set_color(self, main_cpp):
        """`lvgl.color_picker.update` used to fail outright on the missing width."""
        assert main_cpp.count("picker_fixed->set_color(") == 2
