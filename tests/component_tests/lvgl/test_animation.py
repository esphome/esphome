"""Tests for the LVGL animation schema and configuration validation."""

from __future__ import annotations

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.lvgl.animation import (
    ANIMABLE_STYLES,
    ANIMATION_SCHEMA,
    TIMING_SCHEMA,
    from_to,
    literal_color,
)
from esphome.components.lvgl.defines import LValidator
from esphome.core import Lambda


def _animation(**overrides) -> dict:
    """A minimal valid animation config, with optional overrides applied."""
    config = {
        "id": "anim_id",
        "widgets": [{"id": "widget_id", "x": {"from": 0, "to": 100}}],
    }
    config.update(overrides)
    return config


# ---------------------------------------------------------------------------
# Animatable property set
# ---------------------------------------------------------------------------


class TestAnimableStyles:
    def test_all_entries_are_animatable_validators(self) -> None:
        """Every animatable style must be an LValidator marked animatable."""
        assert ANIMABLE_STYLES
        assert all(
            isinstance(v, LValidator) and v.animatable for v in ANIMABLE_STYLES.values()
        )

    def test_known_animatable_present(self) -> None:
        for prop in ("x", "y", "opa", "bg_color", "transform_rotation"):
            assert prop in ANIMABLE_STYLES

    def test_non_animatable_absent(self) -> None:
        # width/height set size but are not animatable; layout/padding never are.
        for prop in ("width", "height", "radius", "pad_all", "align"):
            assert prop not in ANIMABLE_STYLES


# ---------------------------------------------------------------------------
# Animation schema
# ---------------------------------------------------------------------------


class TestAnimationSchema:
    def test_defaults(self) -> None:
        config = ANIMATION_SCHEMA(_animation())
        assert config["duration"].total_milliseconds == 5000
        assert config["start_delay"].total_milliseconds == 0
        assert config["auto_start"] is False
        assert config["loop"] is False
        assert config["timing"] == []

    def test_values_preserved(self) -> None:
        config = ANIMATION_SCHEMA(
            _animation(duration="2s", start_delay="250ms", auto_start=True, loop=True)
        )
        assert config["duration"].total_milliseconds == 2000
        assert config["start_delay"].total_milliseconds == 250
        assert config["auto_start"] is True
        assert config["loop"] is True

    def test_id_required(self) -> None:
        with pytest.raises((Invalid, MultipleInvalid)):
            ANIMATION_SCHEMA({"widgets": [{"id": "widget_id"}]})

    def test_widgets_required(self) -> None:
        with pytest.raises((Invalid, MultipleInvalid)):
            ANIMATION_SCHEMA({"id": "anim_id"})

    def test_multiple_properties_and_widgets(self) -> None:
        config = ANIMATION_SCHEMA(
            _animation(
                widgets=[
                    {
                        "id": "w1",
                        "x": {"from": 0, "to": 100},
                        "opa": {"from": "0%", "to": "100%"},
                    },
                    {"id": "w2", "y": {"from": 10, "to": 50}},
                ]
            )
        )
        assert len(config["widgets"]) == 2

    def test_unknown_property_rejected(self) -> None:
        with pytest.raises((Invalid, MultipleInvalid)):
            ANIMATION_SCHEMA(
                _animation(widgets=[{"id": "w1", "not_a_style": {"from": 0, "to": 1}}])
            )


class TestAnimatedColorLiteral:
    """A color animated via from/to must be a literal, not a lambda."""

    def test_color_lambda_rejected_directly(self) -> None:
        with pytest.raises(Invalid, match="lambda"):
            literal_color(Lambda("return lv_color_hex(0xFF0000);"))

    def test_color_literal_accepted_directly(self) -> None:
        # A literal color value validates without error.
        literal_color(0xFF0000)

    def test_color_lambda_rejected_in_animation(self) -> None:
        with pytest.raises((Invalid, MultipleInvalid), match="lambda"):
            ANIMATION_SCHEMA(
                _animation(
                    widgets=[
                        {
                            "id": "w1",
                            "text_color": {
                                "from": Lambda("return lv_color_hex(0xFF0000);"),
                                "to": 0x00FF00,
                            },
                        }
                    ]
                )
            )

    def test_color_literals_accepted_in_animation(self) -> None:
        config = ANIMATION_SCHEMA(
            _animation(
                widgets=[{"id": "w1", "text_color": {"from": 0xFF0000, "to": 0x00FF00}}]
            )
        )
        assert config["widgets"][0]["id"].id == "w1"

    def test_non_color_property_allows_lambda(self) -> None:
        # Only colors are restricted; numeric properties may use lambdas.
        config = ANIMATION_SCHEMA(
            _animation(
                widgets=[{"id": "w1", "x": {"from": Lambda("return 5;"), "to": 100}}]
            )
        )
        assert config["widgets"][0]["id"].id == "w1"


class TestFromTo:
    def test_requires_both(self) -> None:
        validator = from_to(lambda value: value)
        with pytest.raises((Invalid, MultipleInvalid)):
            validator({"from": 1})
        with pytest.raises((Invalid, MultipleInvalid)):
            validator({"to": 1})

    def test_accepts_both(self) -> None:
        validator = from_to(lambda value: value)
        assert validator({"from": 1, "to": 2}) == {"from": 1, "to": 2}


# ---------------------------------------------------------------------------
# Timing schema
# ---------------------------------------------------------------------------


class TestTimingSchema:
    def test_round_trip_string(self) -> None:
        assert TIMING_SCHEMA("round_trip")["type"] == "round_trip"

    def test_ease_in_out_default_weight(self) -> None:
        result = TIMING_SCHEMA("ease_in_out")
        assert result["type"] == "ease_in_out"
        assert result["weight"] == pytest.approx(2.0)

    def test_ease_in_out_custom_weight(self) -> None:
        result = TIMING_SCHEMA({"type": "ease_in_out", "weight": 3})
        assert result["weight"] == pytest.approx(3.0)

    def test_gravity_defaults(self) -> None:
        result = TIMING_SCHEMA("gravity")
        assert result["type"] == "gravity"
        assert result["bounce"] == pytest.approx(0.5)
        assert result["acceleration"] == pytest.approx(0.5)

    def test_gravity_custom(self) -> None:
        result = TIMING_SCHEMA({"type": "gravity", "bounce": 0.3, "acceleration": 0.8})
        assert result["bounce"] == pytest.approx(0.3)
        assert result["acceleration"] == pytest.approx(0.8)

    def test_unknown_type_rejected(self) -> None:
        with pytest.raises((Invalid, MultipleInvalid)):
            TIMING_SCHEMA({"type": "not_a_timing"})

    def test_timing_list_in_animation(self) -> None:
        config = ANIMATION_SCHEMA(
            _animation(timing=["round_trip", {"type": "gravity", "bounce": 0.3}])
        )
        types = [t["type"] for t in config["timing"]]
        assert types == ["round_trip", "gravity"]
