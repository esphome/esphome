"""Tests for the calibrate_linear least-squares fit."""

import pytest

from esphome.components.sensor import fit_linear


def test_fit_linear_linear_input_output() -> None:
    """A regular dataset still fits y = 2x + 1."""
    k, b = fit_linear([1.0, 2.0, 3.0], [3.0, 5.0, 7.0])
    assert k == pytest.approx(2.0)
    assert b == pytest.approx(1.0)


def test_fit_linear_constant_zero_output() -> None:
    """All 'to' values equal to 0 must fit the constant line y = 0.

    Zero-calibrating a sensor is a valid configuration, but a constant y
    vector has zero variance, so the correlation coefficient used to divide
    by zero instead of returning the flat fit.
    """
    k, b = fit_linear([1.0, 2.0, 3.0], [0.0, 0.0, 0.0])
    assert k == 0.0
    assert b == 0.0


def test_fit_linear_constant_nonzero_output() -> None:
    """All 'to' values equal to a constant must fit y = that constant."""
    k, b = fit_linear([1.0, 2.0, 3.0], [5.0, 5.0, 5.0])
    assert k == 0.0
    assert b == pytest.approx(5.0)
