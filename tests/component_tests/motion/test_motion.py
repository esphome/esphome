"""Tests for the motion component."""

from __future__ import annotations

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.motion import _axis_map, _axis_map_to_matrix
from esphome.components.motion.sensor import (
    _ACCELERATIONS,
    _ANGULAR_RATES,
    _GYROSCOPES,
    CONF_PITCH,
    CONF_ROLL,
    CONFIG_SCHEMA,
    build_sensor_expr,
)
from esphome.cpp_generator import MockObj

# --- Axis map validation ---


class TestAxisMapValidation:
    """Tests for the _axis_map validator."""

    def test_identity_map(self):
        result = _axis_map({"x": "x", "y": "y", "z": "z"})
        assert result == {"x": "x", "y": "y", "z": "z"}

    def test_axis_swap(self):
        result = _axis_map({"x": "y", "y": "z", "z": "x"})
        assert result == {"x": "y", "y": "z", "z": "x"}

    def test_negation(self):
        result = _axis_map({"x": "-y", "y": "z", "z": "x"})
        assert result == {"x": "-y", "y": "z", "z": "x"}

    def test_plus_prefix(self):
        result = _axis_map({"x": "+y", "y": "z", "z": "x"})
        assert result == {"x": "+y", "y": "z", "z": "x"}

    def test_case_insensitive(self):
        result = _axis_map({"x": "X", "y": "Y", "z": "Z"})
        assert result == {"x": "X", "y": "Y", "z": "Z"}

    def test_invalid_axis_value(self):
        with pytest.raises(MultipleInvalid):
            _axis_map({"x": "a", "y": "y", "z": "z"})

    def test_duplicate_mapping(self):
        with pytest.raises(MultipleInvalid):
            _axis_map({"x": "x", "y": "x", "z": "z"})

    def test_all_same_axis(self):
        with pytest.raises(MultipleInvalid):
            _axis_map({"x": "x", "y": "x", "z": "x"})

    def test_empty_value(self):
        with pytest.raises(MultipleInvalid):
            _axis_map({"x": "", "y": "y", "z": "z"})

    def test_invalid_and_duplicate(self):
        """Both invalid value and duplicate should produce multiple errors."""
        with pytest.raises(MultipleInvalid) as exc_info:
            _axis_map({"x": "a", "y": "x", "z": "z"})
        # Should have at least the invalid regex error and the duplicate error
        assert len(exc_info.value.errors) >= 2


# --- Axis map to matrix ---


class TestAxisMapToMatrix:
    """Tests for _axis_map_to_matrix conversion."""

    def test_identity(self):
        assert _axis_map_to_matrix({"x": "x", "y": "y", "z": "z"}) == [
            1,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            1,
        ]

    def test_swap_xy(self):
        # x←y, y←x, z←z
        assert _axis_map_to_matrix({"x": "y", "y": "x", "z": "z"}) == [
            0,
            1,
            0,
            1,
            0,
            0,
            0,
            0,
            1,
        ]

    def test_rotate_xyz(self):
        # x←y, y←z, z←x
        assert _axis_map_to_matrix({"x": "y", "y": "z", "z": "x"}) == [
            0,
            1,
            0,
            0,
            0,
            1,
            1,
            0,
            0,
        ]

    def test_negate_x(self):
        assert _axis_map_to_matrix({"x": "-x", "y": "y", "z": "z"}) == [
            -1,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            1,
        ]

    def test_negate_z(self):
        assert _axis_map_to_matrix({"x": "x", "y": "y", "z": "-z"}) == [
            1,
            0,
            0,
            0,
            1,
            0,
            0,
            0,
            -1,
        ]

    def test_swap_and_negate(self):
        # x←-y, y←z, z←x
        assert _axis_map_to_matrix({"x": "-y", "y": "z", "z": "x"}) == [
            0,
            -1,
            0,
            0,
            0,
            1,
            1,
            0,
            0,
        ]

    def test_plus_prefix_ignored(self):
        assert _axis_map_to_matrix({"x": "+y", "y": "z", "z": "x"}) == [
            0,
            1,
            0,
            0,
            0,
            1,
            1,
            0,
            0,
        ]


# --- Sensor expression generation ---


def _expr_str(sensor_type: str) -> str:
    """Build a sensor expression via the production function and return its string form."""
    return str(build_sensor_expr(sensor_type, MockObj("data")))


class TestSensorExpressions:
    """Tests that sensor code generation produces correct C++ expressions."""

    @pytest.mark.parametrize(
        "sensor_type,expected_index",
        [
            ("acceleration_x", 0),
            ("acceleration_y", 1),
            ("acceleration_z", 2),
        ],
    )
    def test_acceleration_sensors(self, sensor_type, expected_index):
        assert _expr_str(sensor_type) == f"data.acceleration[{expected_index}]"

    @pytest.mark.parametrize(
        "sensor_type,expected_index",
        [
            ("angular_rate_x", 0),
            ("angular_rate_y", 1),
            ("angular_rate_z", 2),
        ],
    )
    def test_angular_rate_sensors(self, sensor_type, expected_index):
        assert _expr_str(sensor_type) == f"data.angular_rate[{expected_index}]"

    @pytest.mark.parametrize(
        "sensor_type,expected_index",
        [
            ("gyroscope_x", 0),
            ("gyroscope_y", 1),
            ("gyroscope_z", 2),
        ],
    )
    def test_gyroscope_maps_to_angular_rate(self, sensor_type, expected_index):
        """Gyroscope sensor types should be remapped to angular_rate in the expression."""
        assert _expr_str(sensor_type) == f"data.angular_rate[{expected_index}]"

    def test_roll_expression(self):
        expr = _expr_str("roll")
        assert "std::atan2f" in expr
        assert "data.acceleration[1]" in expr
        assert "data.acceleration[2]" in expr
        assert "180.0f" in expr
        assert "std::numbers::pi_v<float>" in expr
        # Roll should NOT reference acceleration[0]
        assert "data.acceleration[0]" not in expr

    def test_pitch_expression(self):
        expr = _expr_str("pitch")
        assert "std::atan2f" in expr
        assert "std::sqrtf" in expr
        # All three axes used
        assert "data.acceleration[0]" in expr
        assert "data.acceleration[1]" in expr
        assert "data.acceleration[2]" in expr
        assert "180.0f" in expr
        assert "std::numbers::pi_v<float>" in expr
        # Pitch negates the x component
        assert "(-data.acceleration[0])" in expr


# --- Sensor config schema type validation ---


class TestSensorConfigSchema:
    """Tests for sensor CONFIG_SCHEMA type key validation."""

    def test_invalid_type_rejected(self):
        with pytest.raises((Invalid, MultipleInvalid), match="Unknown value"):
            CONFIG_SCHEMA({"type": "invalid_type"})

    def test_missing_type_rejected(self):
        with pytest.raises((Invalid, MultipleInvalid)):
            CONFIG_SCHEMA({})

    @pytest.mark.parametrize(
        "sensor_type",
        _ACCELERATIONS + _GYROSCOPES + _ANGULAR_RATES + [CONF_PITCH, CONF_ROLL],
    )
    def test_valid_types_accepted(self, sensor_type):
        """Valid sensor types should pass type validation (errors from missing
        required fields like motion_id are expected and acceptable)."""
        try:
            CONFIG_SCHEMA({"type": sensor_type})
        except (Invalid, MultipleInvalid) as e:
            # Should NOT be a type validation error
            assert "Unknown value" not in str(e), (
                f"Type '{sensor_type}' was rejected as unknown"
            )
