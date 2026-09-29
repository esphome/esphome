"""Tests for LVGL action lambda dependency discovery."""

from __future__ import annotations

from esphome.components.lvgl.automation import _iter_lambdas
from esphome.core import Lambda


def test_iter_lambdas_finds_scalar_and_nested_values() -> None:
    scalar = Lambda("return id(sensor_a).state;")
    nested = Lambda("return id(sensor_b).state;")
    config = {"value": scalar, "values": [1, nested], "point": {"x": nested}}

    assert list(_iter_lambdas(config)) == [scalar, nested, nested]


def test_iter_lambdas_ignores_non_lambda_leaves() -> None:
    assert list(_iter_lambdas({"values": [1, "plain", None]})) == []
