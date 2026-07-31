"""Tests for the LVGL color picker widget schema and code generation."""

from __future__ import annotations

import re

import pytest

from esphome.components.lvgl.schemas import container_schema
from esphome.components.lvgl.widgets.color_picker import color_picker_spec
from esphome.config_validation import Invalid
from esphome.const import CONF_WIDTH

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

    def test_width_defaults_to_size_content(self, schema):
        assert schema({})[CONF_WIDTH] == "LV_SIZE_CONTENT"

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


class TestColorPickerSliderChoice:
    """Test choosing which of the six sliders the widget is built from."""

    @pytest.fixture()
    def schema(self):
        return container_schema(color_picker_spec)

    def test_all_sliders_by_default(self, schema):
        assert schema({"width": 100})["sliders"] == [
            "hue",
            "saturation",
            "brightness",
            "red",
            "green",
            "blue",
        ]

    @pytest.mark.parametrize(
        ("configured", "expected"),
        [
            ("rgb", ["red", "green", "blue"]),
            ("hsv", ["hue", "saturation", "brightness"]),
            # Both spellings of the third HSV component are accepted.
            ("hsb", ["hue", "saturation", "brightness"]),
            (["hue", "saturation"], ["hue", "saturation"]),
            (["hue"], ["hue"]),
            # A group can be mixed with single sliders.
            (["rgb", "hue"], ["hue", "red", "green", "blue"]),
        ],
    )
    def test_groups_are_expanded(self, schema, configured, expected):
        assert schema({"width": 100, "sliders": configured})["sliders"] == expected

    def test_order_follows_the_widget_not_the_config(self, schema):
        """The layout is fixed, so the order they are listed in makes no difference."""
        assert schema({"width": 100, "sliders": ["blue", "red"]})["sliders"] == [
            "red",
            "blue",
        ]

    def test_repeats_are_dropped(self, schema):
        assert schema({"width": 100, "sliders": ["red", "rgb"]})["sliders"] == [
            "red",
            "green",
            "blue",
        ]

    def test_empty_list_is_rejected(self, schema):
        with pytest.raises(Invalid, match="At least one slider is required"):
            schema({"width": 100, "sliders": []})

    def test_unknown_slider_is_rejected(self, schema):
        with pytest.raises(Invalid, match="Unknown value 'cyan'"):
            schema({"width": 100, "sliders": ["cyan"]})

    def test_sliders_cannot_be_updated(self):
        """The layout is built from them, so they cannot change after creation."""
        assert "sliders" not in str(color_picker_spec.modify_schema.schema)


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
# applied to each of its sliders instead.
_SLIDER_STYLE_RE = re.compile(
    r"lv_obj_set_style_(\w+)\((\w+)->get_slider\(LvColorPickerType::SLIDER_(\w+)\), "
    r"(.+?), (\w+)\);"
)

ALL_SLIDERS = {"HUE", "SATURATION", "BRIGHTNESS", "RED", "GREEN", "BLUE"}


def _slider_styles(main_cpp: str, var: str) -> dict[tuple[str, str], set[str]]:
    """Return {(property, selector): sliders it was applied to} for one widget."""
    found: dict[tuple[str, str], set[str]] = {}
    for m in _SLIDER_STYLE_RE.finditer(main_cpp):
        if m.group(2) == var:
            found.setdefault((m.group(1), m.group(5)), set()).add(m.group(3))
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
        assert styles[prop, selector] == ALL_SLIDERS

    def test_style_skips_sliders_the_widget_lacks(
        self, generate_main, component_config_path
    ):
        """A style must not be applied to a slider that was never created."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        styles = _slider_styles(main_cpp, "picker_hsv")

        assert styles["radius", "LV_PART_KNOB"] == {"HUE", "SATURATION", "BRIGHTNESS"}

    @pytest.mark.parametrize(
        "selector",
        [
            # A part on its own needs no cast...
            "LV_PART_KNOB",
            # ...while a part combined with a state is cast to the type LVGL takes, which is
            # wide enough for both. A state alone would not be.
            "(lv_style_selector_t)((int)LV_PART_KNOB|(int)LV_STATE_PRESSED)",
        ],
    )
    def test_named_style_reaches_every_slider(
        self, generate_main, component_config_path, selector
    ):
        """A style listed under `styles:` is added to each slider, not to the container."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        added = re.findall(
            r"lv_obj_add_style\(picker_styled->get_slider\("
            r"LvColorPickerType::SLIDER_(\w+)\), picker_style, (.+?)\);",
            main_cpp,
        )

        assert {name for name, sel in added if sel == selector} == ALL_SLIDERS

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


class TestColorPickerObjectProperties:
    """The widget's own object is never pressed, so touch settings go to the sliders."""

    @pytest.fixture()
    def main_cpp(self, generate_main, component_config_path) -> str:
        return generate_main(component_config_path("color_picker.yaml"))

    def test_ext_click_area_reaches_the_sliders(self, main_cpp):
        """This is what makes the knobs easier to grab: it widens each slider's touch area."""
        found = re.findall(
            r"lv_obj_set_ext_click_area\(picker_rgb->get_slider\("
            r"LvColorPickerType::SLIDER_(\w+)\), 12\);",
            main_cpp,
        )

        assert set(found) == {"RED", "GREEN", "BLUE"}

    def test_ext_click_area_is_not_left_on_the_widget_itself(self, main_cpp):
        """The container has nothing clickable in it, so setting it there does nothing."""
        assert "lv_obj_set_ext_click_area(picker_rgb->obj" not in main_cpp

    def test_ext_click_area_under_a_part_is_reported(self, main_cpp):
        """It takes no part, so LVGL cannot apply it to one; that must not pass silently."""
        from esphome.components.lvgl.defines import get_warnings

        assert any("'ext_click_area'" in w and "'knob'" in w for w in get_warnings()), (
            get_warnings()
        )


class TestColorPickerSliderConstruction:
    """The chosen sliders are passed to the constructor, before the layout is built."""

    @pytest.fixture()
    def main_cpp(self, generate_main, component_config_path) -> str:
        return generate_main(component_config_path("color_picker.yaml"))

    @pytest.mark.parametrize(
        ("var", "expected"),
        [
            ("picker_rgb", ["RED", "GREEN", "BLUE"]),
            ("picker_hsv", ["HUE", "SATURATION", "BRIGHTNESS"]),
            ("picker_hue", ["HUE"]),
            (
                "picker_fixed",
                ["HUE", "SATURATION", "BRIGHTNESS", "RED", "GREEN", "BLUE"],
            ),
        ],
    )
    def test_constructor_names_the_chosen_sliders(self, main_cpp, var, expected):
        flags = " | ".join(
            f"LvColorPickerType::SLIDER_FLAG_{name}" for name in expected
        )

        assert f"new({var}) LvColorPickerType({flags});" in main_cpp
