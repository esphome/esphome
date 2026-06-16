"""Tests for the motion component."""

from __future__ import annotations

from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from voluptuous import Invalid, MultipleInvalid

from esphome.components.motion import (
    CALIBRATE_ACTION_SCHEMA,
    CLEAR_ACTION_SCHEMA,
    CONF_AXIS_MAP,
    CONF_SAVE,
    CONF_TRANSFORM_MATRIX,
    _axis_map,
    _axis_map_to_matrix,
    _build_calibrate_action,
    _transform_matrix,
    _validate_matrix_options,
    clear_calibration_to_code,
)
from esphome.components.motion.sensor import (
    _ACCELERATIONS,
    _ANGULAR_RATES,
    _GYROSCOPES,
    CONF_PITCH,
    CONF_ROLL,
    CONFIG_SCHEMA,
    build_sensor_expr,
)
from esphome.const import CONF_ID, CONF_ON_ERROR, CONF_ON_SUCCESS
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


# --- Transform matrix validation ---


class TestTransformMatrix:
    """Tests for the _transform_matrix validator."""

    def test_flat_identity(self):
        result = _transform_matrix([1, 0, 0, 0, 1, 0, 0, 0, 1])
        assert result == [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]

    def test_flat_values_converted_to_float(self):
        result = _transform_matrix([1, 2, 3, 4, 5, 6, 7, 8, 9])
        assert all(isinstance(v, float) for v in result)

    def test_nested_3x3(self):
        result = _transform_matrix([[1, 0, 0], [0, 1, 0], [0, 0, 1]])
        assert result == [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]

    def test_nested_3x3_values(self):
        result = _transform_matrix(
            [[0.5, 0.1, -0.2], [-0.1, 0.9, 0.3], [0.2, -0.3, 0.8]]
        )
        assert len(result) == 9
        assert result[0] == pytest.approx(0.5)
        assert result[3] == pytest.approx(-0.1)
        assert result[8] == pytest.approx(0.8)

    def test_flat_wrong_length_short(self):
        with pytest.raises(Invalid, match="exactly 9"):
            _transform_matrix([1, 0, 0])

    def test_flat_wrong_length_long(self):
        with pytest.raises(Invalid, match="exactly 9"):
            _transform_matrix([1] * 12)

    def test_nested_wrong_row_count(self):
        with pytest.raises(Invalid, match="3 rows"):
            _transform_matrix([[1, 0, 0], [0, 1, 0]])

    def test_nested_wrong_column_count(self):
        with pytest.raises(Invalid, match="3 numbers"):
            _transform_matrix([[1, 0], [0, 1, 0], [0, 0, 1]])

    def test_empty_list(self):
        with pytest.raises(Invalid):
            _transform_matrix([])

    def test_not_a_list(self):
        with pytest.raises(Invalid):
            _transform_matrix("identity")


class TestValidateMatrixOptions:
    """Tests for mutual exclusivity of axis_map and transform_matrix."""

    def test_neither_passes(self):
        config = {"some_key": "value"}
        assert _validate_matrix_options(config) is config

    def test_axis_map_only_passes(self):
        config = {CONF_AXIS_MAP: {"x": "x", "y": "y", "z": "z"}}
        assert _validate_matrix_options(config) is config

    def test_transform_matrix_only_passes(self):
        config = {CONF_TRANSFORM_MATRIX: [1, 0, 0, 0, 1, 0, 0, 0, 1]}
        assert _validate_matrix_options(config) is config

    def test_both_raises(self):
        config = {
            CONF_AXIS_MAP: {"x": "x", "y": "y", "z": "z"},
            CONF_TRANSFORM_MATRIX: [1, 0, 0, 0, 1, 0, 0, 0, 1],
        }
        with pytest.raises(Invalid, match="mutually exclusive"):
            _validate_matrix_options(config)


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
        assert "std::atan2" in expr
        assert "data.acceleration[1]" in expr
        assert "data.acceleration[2]" in expr
        assert "180.0f" in expr
        assert "std::numbers::pi_v<float>" in expr
        # Roll should NOT reference acceleration[0]
        assert "data.acceleration[0]" not in expr

    def test_pitch_expression(self):
        expr = _expr_str("pitch")
        assert "std::atan2" in expr
        assert "std::sqrt" in expr
        # All three axes used
        assert "data.acceleration[0]" in expr
        assert "data.acceleration[1]" in expr
        assert "data.acceleration[2]" in expr
        assert "180.0f" in expr
        assert "std::numbers::pi_v<float>" in expr
        # Pitch negates the x component
        assert "(-data.acceleration[0])" in expr


# --- Calibration math ---
#
# Pure-Python reimplementation of the C++ calibration algorithms so we can
# verify the mathematical properties without needing to compile C++.


def _mat_vec(m: list[float], v: list[float]) -> list[float]:
    """Multiply a row-major 3x3 matrix by a 3-vector."""
    return [
        m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
        m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
        m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
    ]


def _mat_mul(a: list[float], b: list[float]) -> list[float]:
    """Multiply two row-major 3x3 matrices."""
    r = [0.0] * 9
    for i in range(3):
        for j in range(3):
            r[i * 3 + j] = sum(a[i * 3 + k] * b[k * 3 + j] for k in range(3))
    return r


def _transpose(m: list[float]) -> list[float]:
    """Transpose a row-major 3x3 matrix."""
    return [m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]]


def _det(m: list[float]) -> float:
    """Determinant of a 3x3 matrix."""
    return (
        m[0] * (m[4] * m[8] - m[5] * m[7])
        - m[1] * (m[3] * m[8] - m[5] * m[6])
        + m[2] * (m[3] * m[7] - m[4] * m[6])
    )


def _calibrate_level(
    raw: list[float], matrix: list[float] | None = None
) -> list[float]:
    """Python port of MotionComponent::calibrate_level.

    Composes the correction with *matrix* (defaults to identity).
    """
    import math

    if matrix is None:
        matrix = list(IDENTITY)

    # Apply current matrix first
    mapped = _mat_vec(matrix, raw)

    nx, ny, nz = mapped
    mag = math.sqrt(nx * nx + ny * ny + nz * nz)
    nx /= mag
    ny /= mag
    nz /= mag

    if nz > 0.9999:
        return matrix[:]  # already aligned, preserve existing matrix

    if nz < -0.9999:
        r = [1, 0, 0, 0, -1, 0, 0, 0, -1]
    else:
        f = 1.0 / (1.0 + nz)
        r = [
            1.0 - nx * nx * f,
            -nx * ny * f,
            -nx,
            -nx * ny * f,
            1.0 - ny * ny * f,
            -ny,
            nx,
            ny,
            nz,
        ]

    return _mat_mul(r, matrix)


def _calibrate_heading(matrix: list[float], raw: list[float]) -> list[float]:
    """Python port of MotionComponent::calibrate_heading."""
    import math

    mapped = _mat_vec(matrix, raw)
    mx, my = mapped[0], mapped[1]
    h = math.sqrt(mx * mx + my * my)
    sign_mx = 1.0 if mx >= 0 else -1.0
    cos_phi = sign_mx * mx / h  # = |mx| / h
    sin_phi = sign_mx * my / h

    old = matrix[:]
    new = old[:]
    new[0] = cos_phi * old[0] + sin_phi * old[3]
    new[1] = cos_phi * old[1] + sin_phi * old[4]
    new[2] = cos_phi * old[2] + sin_phi * old[5]
    new[3] = -sin_phi * old[0] + cos_phi * old[3]
    new[4] = -sin_phi * old[1] + cos_phi * old[4]
    new[5] = -sin_phi * old[2] + cos_phi * old[5]
    return new


IDENTITY = [1, 0, 0, 0, 1, 0, 0, 0, 1]


class TestCalibrateLevel:
    """Verify the Rodrigues-based level calibration matrix."""

    def _assert_maps_to_z(self, raw: list[float]) -> list[float]:
        """Assert that the calibration matrix maps raw to [0, 0, 1]."""
        import math

        m = _calibrate_level(raw)
        mag = math.sqrt(sum(v * v for v in raw))
        norm = [v / mag for v in raw]
        result = _mat_vec(m, norm)
        assert result[0] == pytest.approx(0, abs=1e-6)
        assert result[1] == pytest.approx(0, abs=1e-6)
        assert result[2] == pytest.approx(1, abs=1e-6)
        return m

    def test_already_flat(self):
        m = _calibrate_level([0, 0, 1.0])
        assert m == IDENTITY

    def test_preserves_existing_matrix_when_flat(self):
        """If already flat after axis mapping, level cal should not change the matrix."""
        swap = [0, 1, 0, 1, 0, 0, 0, 0, 1]  # swap X↔Y
        m = _calibrate_level([0, 0, 1.0], swap)
        assert m == swap

    def test_composes_with_existing_matrix(self):
        """Level calibration should correct tilt while preserving an existing axis swap."""
        import math

        swap = [0, 1, 0, 1, 0, 0, 0, 0, 1]  # swap X↔Y
        # Tilted raw: gravity has X component in raw frame
        raw = [0.3, 0.0, 0.954]
        m = _calibrate_level(raw, swap)
        # After calibration, current raw should map to [0, 0, ~1]
        mag = math.sqrt(sum(v * v for v in raw))
        norm = [v / mag for v in raw]
        result = _mat_vec(m, norm)
        assert result[0] == pytest.approx(0, abs=1e-5)
        assert result[1] == pytest.approx(0, abs=1e-5)
        assert result[2] == pytest.approx(1, abs=1e-5)
        # Result should differ from calibrating without the swap
        m_no_swap = _calibrate_level(raw)
        assert m != m_no_swap

    def test_upside_down(self):
        m = _calibrate_level([0, 0, -1.0])
        # 180° about X
        assert m == [1, 0, 0, 0, -1, 0, 0, 0, -1]
        result = _mat_vec(m, [0, 0, -1])
        assert result[2] == pytest.approx(1, abs=1e-6)

    def test_gravity_along_x(self):
        self._assert_maps_to_z([1.0, 0, 0])

    def test_gravity_along_neg_x(self):
        self._assert_maps_to_z([-1.0, 0, 0])

    def test_gravity_along_y(self):
        self._assert_maps_to_z([0, 1.0, 0])

    def test_tilted_45_degrees(self):
        import math

        self._assert_maps_to_z(
            [math.sin(math.radians(45)), 0, math.cos(math.radians(45))]
        )

    def test_arbitrary_vector(self):
        self._assert_maps_to_z([0.3, -0.5, 0.81])

    def test_unnormalized_input(self):
        """Input does not need to be unit length."""
        self._assert_maps_to_z([0.6, -1.0, 1.62])

    @pytest.mark.parametrize(
        "raw",
        [
            [1.0, 0, 0],
            [0, 1.0, 0],
            [0.3, -0.5, 0.81],
            [-0.7, 0.4, 0.59],
        ],
    )
    def test_result_is_proper_rotation(self, raw):
        """The resulting matrix should be orthogonal with determinant +1."""
        m = _calibrate_level(raw)
        # R^T * R ≈ I
        product = _mat_mul(_transpose(m), m)
        for i in range(9):
            expected = 1.0 if i % 4 == 0 else 0.0
            assert product[i] == pytest.approx(expected, abs=1e-6)
        # det ≈ 1
        assert _det(m) == pytest.approx(1.0, abs=1e-6)


class TestCalibrateHeading:
    """Verify the Z-rotation heading correction."""

    def test_y_axis_tilt_no_heading_error(self):
        """Device tilted purely around Y — heading should already be correct."""
        import math

        flat_raw = [0, 0, 1.0]
        level_m = _calibrate_level(flat_raw)
        # Tilt 30° around Y: gravity = [-sin30, 0, cos30]
        tilted_raw = [-math.sin(math.radians(30)), 0, math.cos(math.radians(30))]
        heading_m = _calibrate_heading(level_m, tilted_raw)
        # Matrix should barely change since there's no Y component
        for i in range(9):
            assert heading_m[i] == pytest.approx(level_m[i], abs=1e-6)

    def test_corrects_heading_rotation(self):
        """After level+heading calibration, mapped Y should be ~0 when tilted."""
        import math

        # Simulate a sensor whose chip is rotated 30° around Z relative to enclosure
        angle = math.radians(30)
        # When the enclosure is flat, the raw reading is [0, 0, 1] regardless of Z rotation
        level_m = _calibrate_level([0, 0, 1.0])

        # When tilted around the enclosure's Y axis, the raw reading in the
        # chip frame has both X and Y components due to the Z-rotation offset
        tilt = math.radians(20)
        # In enclosure frame: [-sin(tilt), 0, cos(tilt)]
        # Rotated by Z-angle into chip frame:
        ex = -math.sin(tilt) * math.cos(angle)
        ey = -math.sin(tilt) * math.sin(angle)
        ez = math.cos(tilt)
        tilted_raw = [ex, ey, ez]

        heading_m = _calibrate_heading(level_m, tilted_raw)
        # After correction, mapped Y should be 0
        result = _mat_vec(heading_m, tilted_raw)
        assert result[1] == pytest.approx(0, abs=1e-6)
        # Z should still be correct
        assert result[2] == pytest.approx(math.cos(tilt), abs=1e-6)

    def test_full_calibration_sequence(self):
        """End-to-end: level then heading produces correct frame alignment."""
        import math

        # Chip is mounted tilted 15° around Y and 25° around Z
        # Build the chip-to-enclosure rotation: Rz(25°) * Ry(15°)
        yz = math.radians(25)
        yy = math.radians(15)
        # Ry(yy)
        ry = [
            math.cos(yy),
            0,
            math.sin(yy),
            0,
            1,
            0,
            -math.sin(yy),
            0,
            math.cos(yy),
        ]
        # Rz(yz)
        rz = [
            math.cos(yz),
            -math.sin(yz),
            0,
            math.sin(yz),
            math.cos(yz),
            0,
            0,
            0,
            1,
        ]
        chip_rot = _mat_mul(rz, ry)  # chip orientation in enclosure frame
        # Inverse (transpose) maps enclosure vectors to chip readings
        chip_rot_inv = _transpose(chip_rot)

        # Step 1: Device flat — gravity in enclosure frame is [0, 0, 1]
        flat_raw = _mat_vec(chip_rot_inv, [0, 0, 1])
        level_m = _calibrate_level(flat_raw)

        # After level calibration, flat reading should map to [0, 0, 1]
        check_flat = _mat_vec(level_m, flat_raw)
        assert check_flat[0] == pytest.approx(0, abs=1e-5)
        assert check_flat[1] == pytest.approx(0, abs=1e-5)
        assert check_flat[2] == pytest.approx(1, abs=1e-5)

        # Step 2: Tilt enclosure around Y by 20°
        tilt = math.radians(20)
        tilted_enclosure = [-math.sin(tilt), 0, math.cos(tilt)]
        tilted_raw = _mat_vec(chip_rot_inv, tilted_enclosure)
        heading_m = _calibrate_heading(level_m, tilted_raw)

        # After heading calibration, the mapped reading should be
        # [-sin(tilt), 0, cos(tilt)] — all horizontal component in X
        result = _mat_vec(heading_m, tilted_raw)
        assert result[0] == pytest.approx(-math.sin(tilt), abs=1e-5)
        assert result[1] == pytest.approx(0, abs=1e-5)
        assert result[2] == pytest.approx(math.cos(tilt), abs=1e-5)

    @pytest.mark.parametrize(
        "raw",
        [
            [0.3, -0.5, 0.81],
            [-0.7, 0.4, 0.59],
        ],
    )
    def test_heading_preserves_orthogonality(self, raw):
        """Heading correction composed with level should remain a proper rotation."""

        level_m = _calibrate_level(raw)
        # Create a tilted reading for heading calibration
        tilt_raw = [v + 0.3 for v in raw]  # perturb to get XY component
        heading_m = _calibrate_heading(level_m, tilt_raw)
        product = _mat_mul(_transpose(heading_m), heading_m)
        for i in range(9):
            expected = 1.0 if i % 4 == 0 else 0.0
            assert product[i] == pytest.approx(expected, abs=1e-5)
        assert _det(heading_m) == pytest.approx(1.0, abs=1e-5)


# --- Calibration action schema & codegen ---


class TestCalibrateActionSchema:
    """Tests for the CALIBRATE_ACTION_SCHEMA used by both calibration actions."""

    def test_schema_accepts_on_success_key(self):
        """on_success must be a recognised optional key."""
        schema_keys = {str(k) for k in CALIBRATE_ACTION_SCHEMA.schema}
        assert CONF_ON_SUCCESS in schema_keys

    def test_schema_accepts_on_error_key(self):
        """on_error must be a recognised optional key."""
        schema_keys = {str(k) for k in CALIBRATE_ACTION_SCHEMA.schema}
        assert CONF_ON_ERROR in schema_keys


@pytest.fixture
def mock_codegen():
    """Mock cg and automation functions used by _build_calibrate_action."""
    mock_var = MagicMock()
    mock_parent = MagicMock()

    with (
        patch(
            "esphome.components.motion.cg.get_variable",
            new_callable=AsyncMock,
            return_value=mock_parent,
        ) as mock_get_var,
        patch(
            "esphome.components.motion.cg.new_Pvariable",
            return_value=mock_var,
        ) as mock_new_pvar,
        patch(
            "esphome.components.motion.automation.build_automation",
            new_callable=AsyncMock,
        ) as mock_build_auto,
    ):
        yield {
            "get_variable": mock_get_var,
            "new_Pvariable": mock_new_pvar,
            "build_automation": mock_build_auto,
            "var": mock_var,
            "parent": mock_parent,
        }


@pytest.mark.asyncio
async def test_build_calibrate_action_no_triggers(mock_codegen):
    """Without on_success/on_error, build_automation should not be called."""
    config = {CONF_ID: MagicMock()}
    action_id = MagicMock()
    template_arg = MagicMock()

    result = await _build_calibrate_action(config, action_id, template_arg, [])

    assert result is mock_codegen["var"]
    mock_codegen["new_Pvariable"].assert_called_once_with(
        action_id, template_arg, mock_codegen["parent"]
    )
    mock_codegen["build_automation"].assert_not_called()


@pytest.mark.asyncio
async def test_build_calibrate_action_with_on_success(mock_codegen):
    """on_success should wire build_automation to get_success_trigger()."""
    on_success_config = MagicMock()
    config = {CONF_ID: MagicMock(), CONF_ON_SUCCESS: on_success_config}

    await _build_calibrate_action(config, MagicMock(), MagicMock(), [])

    mock_codegen["build_automation"].assert_called_once_with(
        mock_codegen["var"].get_success_trigger(), [], on_success_config
    )


@pytest.mark.asyncio
async def test_build_calibrate_action_with_on_error(mock_codegen):
    """on_error should wire build_automation to get_error_trigger()."""
    on_error_config = MagicMock()
    config = {CONF_ID: MagicMock(), CONF_ON_ERROR: on_error_config}

    await _build_calibrate_action(config, MagicMock(), MagicMock(), [])

    mock_codegen["build_automation"].assert_called_once_with(
        mock_codegen["var"].get_error_trigger(), [], on_error_config
    )


@pytest.mark.asyncio
async def test_build_calibrate_action_with_both_triggers(mock_codegen):
    """Both on_success and on_error should each produce a build_automation call."""
    on_success_config = MagicMock()
    on_error_config = MagicMock()
    config = {
        CONF_ID: MagicMock(),
        CONF_ON_SUCCESS: on_success_config,
        CONF_ON_ERROR: on_error_config,
    }

    await _build_calibrate_action(config, MagicMock(), MagicMock(), [])

    assert mock_codegen["build_automation"].call_count == 2
    calls = mock_codegen["build_automation"].call_args_list
    # First call: on_success
    assert calls[0].args == (
        mock_codegen["var"].get_success_trigger(),
        [],
        on_success_config,
    )
    # Second call: on_error
    assert calls[1].args == (
        mock_codegen["var"].get_error_trigger(),
        [],
        on_error_config,
    )


# --- Clear calibration action ---


class TestClearActionSchema:
    """Tests for CLEAR_ACTION_SCHEMA."""

    def test_schema_has_save_key(self):
        schema_keys = {str(k) for k in CLEAR_ACTION_SCHEMA.schema}
        assert CONF_SAVE in schema_keys

    def test_save_defaults_to_false(self):
        result = CLEAR_ACTION_SCHEMA({CONF_ID: "x"})
        assert result[CONF_SAVE] is False


@pytest.fixture
def mock_clear_codegen():
    """Mock cg functions used by clear_calibration_to_code."""
    mock_var = MagicMock()
    mock_parent = MagicMock()
    with (
        patch(
            "esphome.components.motion.cg.get_variable",
            new_callable=AsyncMock,
            return_value=mock_parent,
        ),
        patch(
            "esphome.components.motion.cg.new_Pvariable",
            return_value=mock_var,
        ) as mock_new_pvar,
        patch("esphome.components.motion.cg.add") as mock_add,
    ):
        yield {"new_Pvariable": mock_new_pvar, "add": mock_add, "var": mock_var}


@pytest.mark.asyncio
async def test_clear_action_without_save(mock_clear_codegen):
    """With save=False, set_save should not be emitted."""
    config = {CONF_ID: MagicMock(), CONF_SAVE: False}
    result = await clear_calibration_to_code(config, MagicMock(), MagicMock(), [])
    assert result is mock_clear_codegen["var"]
    mock_clear_codegen["add"].assert_not_called()


@pytest.mark.asyncio
async def test_clear_action_with_save(mock_clear_codegen):
    """With save=True, set_save(True) should be emitted exactly once."""
    config = {CONF_ID: MagicMock(), CONF_SAVE: True}
    await clear_calibration_to_code(config, MagicMock(), MagicMock(), [])
    mock_clear_codegen["var"].set_save.assert_called_once_with(True)
    mock_clear_codegen["add"].assert_called_once()


# --- Calibration persistence invalidation ---
#
# The C++ side stores a hash of the build-time base matrix alongside the saved
# calibration so a changed axis_map invalidates stale NVS data without orphaning
# storage (the pref key stays ID-stable). These tests pin the design properties
# of that base-matrix fingerprint: deterministic for identical maps, distinct
# for different ones.


def _hash_matrix(matrix: list[float]) -> int:
    """Python port of the C++ hash_matrix() (FNV-1a over the float bytes)."""
    import struct

    data = struct.pack("<9f", *matrix)
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


class TestBaseMatrixHash:
    """Properties of the base-matrix fingerprint used for NVS invalidation."""

    def test_identical_axis_maps_hash_equal(self):
        a = _axis_map_to_matrix({"x": "x", "y": "y", "z": "z"})
        b = _axis_map_to_matrix({"x": "x", "y": "y", "z": "z"})
        assert _hash_matrix([float(v) for v in a]) == _hash_matrix(
            [float(v) for v in b]
        )

    def test_different_axis_maps_hash_differ(self):
        identity = _axis_map_to_matrix({"x": "x", "y": "y", "z": "z"})
        swapped = _axis_map_to_matrix({"x": "y", "y": "x", "z": "z"})
        assert _hash_matrix([float(v) for v in identity]) != _hash_matrix(
            [float(v) for v in swapped]
        )

    def test_sign_change_hashes_differ(self):
        pos = _axis_map_to_matrix({"x": "x", "y": "y", "z": "z"})
        neg = _axis_map_to_matrix({"x": "-x", "y": "y", "z": "z"})
        assert _hash_matrix([float(v) for v in pos]) != _hash_matrix(
            [float(v) for v in neg]
        )


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
