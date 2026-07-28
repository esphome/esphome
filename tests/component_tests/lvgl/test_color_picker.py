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

    def test_slider_parts_are_declared(self):
        """`items` and `knob` stand for the sliders the widget is built from."""
        assert color_picker_spec.parts == ("main", "items", "knob")

    @pytest.mark.parametrize("part", ["items", "knob"])
    def test_slider_parts_are_accepted(self, schema, part):
        assert schema({"width": 100, part: {"radius": 4}}) is not None


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


# The widget's own object is a plain container, so styles for the parts a slider has are
# applied to each of the six sliders instead.
_SLIDER_STYLE_RE = re.compile(
    r"lv_obj_set_style_(\w+)\((\w+)->get_slider\((\d)\), (.+?), (\w+)\);"
)

SLIDER_COUNT = 6


def _slider_styles(main_cpp: str, var: str) -> dict[tuple[str, str], set[int]]:
    """Return {(property, selector): sliders it was applied to} for one widget."""
    found: dict[tuple[str, str], set[int]] = {}
    for m in _SLIDER_STYLE_RE.finditer(main_cpp):
        if m.group(2) == var:
            found.setdefault((m.group(1), m.group(5)), set()).add(int(m.group(3)))
    return found


class TestColorPickerSliderStyles:
    @pytest.fixture()
    def styles(self, generate_main, component_config_path):
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        return _slider_styles(main_cpp, "picker_styled")

    @pytest.mark.parametrize(
        ("prop", "selector"),
        [
            # `knob` keeps its name on the slider...
            ("bg_color", "LV_PART_KNOB"),
            ("radius", "LV_PART_KNOB"),
            # ...while `items` becomes the slider's own body.
            ("bg_opa", "LV_PART_MAIN"),
            # A state without a part names only the state, as both are zero.
            ("border_width", "LV_STATE_PRESSED"),
        ],
    )
    def test_style_reaches_every_slider(self, styles, prop, selector):
        assert styles[prop, selector] == set(range(SLIDER_COUNT))

    def test_no_styles_are_left_on_the_widget_itself(
        self, generate_main, component_config_path
    ):
        """Setting these on the container would have no visible effect."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        own = _styles_for(main_cpp, "picker_styled")

        assert set(own) == {"width", "height"}

    def test_knob_tinting_is_disabled_by_a_knob_background(
        self, generate_main, component_config_path
    ):
        """The tint is a local style, so it would overwrite the configured colour."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))

        assert "picker_styled->set_tint_knobs(false)" in main_cpp

    @pytest.mark.parametrize(
        "var", ["picker_content", "picker_fixed", "picker_colour_id"]
    )
    def test_knob_tinting_is_left_on_otherwise(
        self, generate_main, component_config_path, var
    ):
        main_cpp = generate_main(component_config_path("color_picker.yaml"))

        assert f"{var}->set_tint_knobs(" not in main_cpp
