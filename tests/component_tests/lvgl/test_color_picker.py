"""Tests for the LVGL color picker widget schema and code generation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest
from voluptuous import Schema

from esphome.components.lvgl.schemas import container_schema
from esphome.components.lvgl.widgets import _build_update_schema
from esphome.components.lvgl.widgets.color_picker import color_picker_spec
from esphome.config_validation import Invalid
from esphome.const import CONF_WIDTH


@pytest.fixture()
def update_schema() -> Schema:
    """The schema behind `lvgl.color_picker.update`."""
    return _build_update_schema(color_picker_spec)


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


class TestColorPickerValidation:
    """Test the widget's own schema options."""

    @pytest.fixture()
    def schema(self) -> Callable[[dict], dict]:
        return container_schema(color_picker_spec)

    @pytest.mark.parametrize("width", [100, "50%", "SIZE_CONTENT"])
    def test_width_accepted(
        self, schema: Callable[[dict], dict], width: int | str
    ) -> None:
        assert schema({"width": width}) is not None

    def test_width_defaults_to_size_content(
        self, schema: Callable[[dict], dict]
    ) -> None:
        assert schema({})[CONF_WIDTH] == "LV_SIZE_CONTENT"

    def test_height_is_rejected(self, schema: Callable[[dict], dict]) -> None:
        """The widget is square, so a height of its own makes no sense."""
        with pytest.raises(Invalid, match="Height will be set to the same as width"):
            schema({"width": 100, "height": 100})

    def test_color_is_optional(self, schema: Callable[[dict], dict]) -> None:
        assert "color" not in schema({"width": 100})

    def test_on_value_is_accepted(self, schema: Callable[[dict], dict]) -> None:
        """The trigger is only offered if the widget declares it has a value."""
        result = schema({"width": 100, "on_value": [{"logger.log": "picked"}]})

        assert "on_value" in result

    def test_value_argument_is_a_color(self) -> None:
        """`x` must be an esphome Color, so that lambdas can use x.r/x.g/x.b.

        esphome::Color converts to lv_color_t implicitly, so declaring the wrong
        one of the two still compiles but gives lambdas the LVGL struct, whose
        members are named differently.
        """
        (arg_type,) = color_picker_spec.w_type.get_arg_type()

        assert str(arg_type) == "Color"

    def test_update_action_sets_the_colour(self, update_schema: Schema) -> None:
        """The colour is the one thing an update action is for."""
        assert update_schema({"id": "picker", "color": 0xFF8800}) is not None

    def test_update_action_does_not_require_width(self, update_schema: Schema) -> None:
        """Width is only needed when the widget is created, unlike in the widget schema."""
        assert CONF_WIDTH not in update_schema({"id": "picker", "color": 0xFF8800})

    def test_slider_parts_are_declared(self) -> None:
        """`items` and `knob` stand for the sliders the widget is built from."""
        assert color_picker_spec.parts == ("main", "items", "knob")

    @pytest.mark.parametrize("part", ["items", "knob"])
    def test_slider_parts_are_accepted(
        self, schema: Callable[[dict], dict], part: str
    ) -> None:
        assert schema({"width": 100, part: {"radius": 4}}) is not None


class TestColorPickerSliderChoice:
    """Test choosing which of the six sliders the widget is built from."""

    @pytest.fixture()
    def schema(self) -> Callable[[dict], dict]:
        return container_schema(color_picker_spec)

    def test_all_sliders_by_default(self, schema: Callable[[dict], dict]) -> None:
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
    def test_groups_are_expanded(
        self,
        schema: Callable[[dict], dict],
        configured: str | list[str],
        expected: list[str],
    ) -> None:
        assert schema({"width": 100, "sliders": configured})["sliders"] == expected

    def test_order_follows_the_widget_not_the_config(
        self, schema: Callable[[dict], dict]
    ) -> None:
        """The layout is fixed, so the order they are listed in makes no difference."""
        assert schema({"width": 100, "sliders": ["blue", "red"]})["sliders"] == [
            "red",
            "blue",
        ]

    def test_repeats_are_dropped(self, schema: Callable[[dict], dict]) -> None:
        assert schema({"width": 100, "sliders": ["red", "rgb"]})["sliders"] == [
            "red",
            "green",
            "blue",
        ]

    def test_empty_list_is_rejected(self, schema: Callable[[dict], dict]) -> None:
        with pytest.raises(Invalid, match="At least one slider is required"):
            schema({"width": 100, "sliders": []})

    def test_unknown_slider_is_rejected(self, schema: Callable[[dict], dict]) -> None:
        with pytest.raises(Invalid, match="Unknown value 'cyan'"):
            schema({"width": 100, "sliders": ["cyan"]})

    def test_sliders_cannot_be_updated(self, update_schema: Schema) -> None:
        """The layout is built from them, so they cannot change after creation."""
        with pytest.raises(Invalid, match="sliders"):
            update_schema({"id": "picker", "sliders": "rgb"})


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
    def main_cpp(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> str:
        return generate_main(component_config_path("color_picker.yaml"))

    def test_height_follows_width(self, main_cpp: str) -> None:
        """The widget is square, so height is generated to match the given width."""
        for var, expected in (
            ("picker_content", "LV_SIZE_CONTENT"),
            ("picker_fixed", "120"),
            ("picker_colour_id", "lv_pct(50)"),
        ):
            styles = _styles_for(main_cpp, var)
            assert styles["width"] == expected
            assert styles["height"] == expected

    def test_color_is_applied(self, main_cpp: str) -> None:
        assert "picker_fixed->set_color(" in main_cpp
        assert "picker_colour_id->set_color(" in main_cpp

    def test_color_is_omitted_when_not_configured(self, main_cpp: str) -> None:
        assert "picker_content->set_color(" not in main_cpp

    def test_black_is_applied(self, main_cpp: str) -> None:
        """A hex colour validates to a plain int, and black must not read as absent."""
        assert "picker_black->set_color(lv_color_make(0, 0, 0))" in main_cpp

    def test_trigger_uses_color_type(self, main_cpp: str) -> None:
        """The on_value trigger is declared with the Color the widget reports."""
        assert "Trigger<Color, lv_event_t *>" in main_cpp

    def test_trigger_reads_the_current_color(self, main_cpp: str) -> None:
        assert "picker_colour_id->get_color()" in main_cpp

    def test_update_action_generates_set_color(self, main_cpp: str) -> None:
        """`lvgl.color_picker.update` used to fail outright on the missing width."""
        assert main_cpp.count("picker_fixed->set_color(") == 2

    def test_gradient_stops_are_raised_for_the_hue_bar(self, main_cpp: str) -> None:
        """The hue bar fills seven stops of a fixed-size array.

        `gradient.py` raises the limit when a colour picker is in use, and the C++
        `static_assert` relies on that having happened. It works because the widget
        records the use during validation, which runs first, but nothing in either
        file says so, hence this.
        """
        from esphome.components.lvgl.defines import get_defines

        assert get_defines()["LV_GRADIENT_MAX_STOPS"] == "7"

    def test_gradient_stops_are_left_alone_without_a_picker(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> None:
        """The other half of the same invariant: only a picker pays for the extra stops."""
        from esphome.components.lvgl.defines import get_defines

        # Any configuration without a colour picker in it will do.
        generate_main(component_config_path("named_styles.yaml"))

        assert get_defines()["LV_GRADIENT_MAX_STOPS"] == "2"


# The widget's own object is a plain container, so styles for the parts a slider has are
# applied to each of its sliders instead.
_SLIDER_STYLE_RE = re.compile(
    r"lv_obj_set_style_(\w+)\((\w+)->get_slider\(LvColorPickerType::SLIDER_(\w+)\), "
    r"(.+?), (\w+)\);"
)

ALL_SLIDERS = {"HUE", "SATURATION", "BRIGHTNESS", "RED", "GREEN", "BLUE"}


def _slider_style_values(main_cpp: str, var: str, prop: str) -> set[str]:
    """Return every value one style property was set to across a widget's sliders."""
    return {
        m.group(4)
        for m in _SLIDER_STYLE_RE.finditer(main_cpp)
        if m.group(2) == var and m.group(1) == prop and m.group(5) == "LV_PART_KNOB"
    }


def _slider_styles(main_cpp: str, var: str) -> dict[tuple[str, str], set[str]]:
    """Return {(property, selector): sliders it was applied to} for one widget."""
    found: dict[tuple[str, str], set[str]] = {}
    for m in _SLIDER_STYLE_RE.finditer(main_cpp):
        if m.group(2) == var:
            found.setdefault((m.group(1), m.group(5)), set()).add(m.group(3))
    return found


class TestColorPickerSliderStyles:
    @pytest.fixture()
    def styles(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> dict[tuple[str, str], set[str]]:
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
    def test_style_reaches_every_slider(
        self, styles: dict[tuple[str, str], set[str]], prop: str, selector: str
    ) -> None:
        assert styles[prop, selector] == ALL_SLIDERS

    def test_style_skips_sliders_the_widget_lacks(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> None:
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
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
        selector: str,
    ) -> None:
        """A style listed under `styles:` is added to each slider, not to the container."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        added = re.findall(
            r"lv_obj_add_style\(picker_styled->get_slider\("
            r"LvColorPickerType::SLIDER_(\w+)\), picker_style, (.+?)\);",
            main_cpp,
        )

        assert {name for name, sel in added if sel == selector} == ALL_SLIDERS

    def test_a_lambda_is_generated_once_for_all_the_sliders(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> None:
        """One YAML value must not become one C++ function per slider."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))

        # The source also appears once in the comment codegen writes above the lambda.
        assert main_cpp.count("int border_width_VAR_ = []() -> int {") == 1

    def test_the_same_lambda_reaches_every_slider(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> None:
        """Generating it once must not mean applying it once."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        styles = _slider_styles(main_cpp, "picker_styled")

        assert styles["border_width", "LV_PART_KNOB"] == ALL_SLIDERS
        assert _slider_style_values(main_cpp, "picker_styled", "border_width") == {
            "border_width_VAR_"
        }

    def test_no_styles_are_left_on_the_widget_itself(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> None:
        """Setting these on the container would have no visible effect."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))
        own = _styles_for(main_cpp, "picker_styled")

        assert set(own) == {"width", "height"}

    @pytest.mark.parametrize(
        "var",
        [
            # The background is set on the knob directly...
            "picker_styled",
            # ...or through a named style, which the tint would still outrank.
            "picker_knob_style",
        ],
    )
    def test_knob_tinting_is_disabled_by_a_knob_background(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
        var: str,
    ) -> None:
        """The tint is a local style, so it would overwrite the configured colour."""
        main_cpp = generate_main(component_config_path("color_picker.yaml"))

        assert f"{var}->set_tint_knobs(false)" in main_cpp

    @pytest.mark.parametrize(
        "var", ["picker_content", "picker_fixed", "picker_colour_id"]
    )
    def test_knob_tinting_is_left_on_otherwise(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
        var: str,
    ) -> None:
        main_cpp = generate_main(component_config_path("color_picker.yaml"))

        assert f"{var}->set_tint_knobs(" not in main_cpp


class TestColorPickerObjectProperties:
    """The widget's own object is never pressed, so touch settings go to the sliders."""

    @pytest.fixture()
    def main_cpp(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> str:
        return generate_main(component_config_path("color_picker.yaml"))

    def test_ext_click_area_reaches_the_sliders(self, main_cpp: str) -> None:
        """This is what makes the knobs easier to grab: it widens each slider's touch area."""
        found = re.findall(
            r"lv_obj_set_ext_click_area\(picker_rgb->get_slider\("
            r"LvColorPickerType::SLIDER_(\w+)\), 12\);",
            main_cpp,
        )

        assert set(found) == {"RED", "GREEN", "BLUE"}

    def test_ext_click_area_is_not_left_on_the_widget_itself(
        self, main_cpp: str
    ) -> None:
        """The container has nothing clickable in it, so setting it there does nothing."""
        assert "lv_obj_set_ext_click_area(picker_rgb->obj" not in main_cpp

    def test_ext_click_area_under_a_part_is_reported(self, main_cpp: str) -> None:
        """It takes no part, so LVGL cannot apply it to one; that must not pass silently."""
        from esphome.components.lvgl.defines import get_warnings

        assert any("'ext_click_area'" in w and "'knob'" in w for w in get_warnings()), (
            get_warnings()
        )

    def test_scrollbar_mode_stays_on_the_widget_itself(self, main_cpp: str) -> None:
        """Only the touch margin moves: a slider never scrolls, but the container can."""
        assert "lv_obj_set_scrollbar_mode(picker_rgb->obj, LV_SCROLLBAR_MODE_OFF)" in (
            main_cpp
        )

    def test_scrollbar_mode_does_not_reach_the_sliders(self, main_cpp: str) -> None:
        assert "lv_obj_set_scrollbar_mode(picker_rgb->get_slider(" not in main_cpp


class TestColorPickerSliderConstruction:
    """The chosen sliders are passed to the constructor, before the layout is built."""

    @pytest.fixture()
    def main_cpp(
        self,
        generate_main: Callable[[str | Path], str],
        component_config_path: Callable[[str], Path],
    ) -> str:
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
    def test_constructor_names_the_chosen_sliders(
        self, main_cpp: str, var: str, expected: list[str]
    ) -> None:
        flags = " | ".join(
            f"LvColorPickerType::SLIDER_FLAG_{name}" for name in expected
        )

        assert f"new({var}) LvColorPickerType({flags});" in main_cpp
