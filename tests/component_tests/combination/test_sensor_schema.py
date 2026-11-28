import pytest

from esphome import config_validation as cv
from esphome.components.combination.sensor import (
    CONFIG_SCHEMA,
    WEIGHTED_MEAN_SOURCE_SCHEMA,
)
from esphome.core import Lambda


@pytest.mark.parametrize(
    "config",
    [
        {"type": "kalman", "name": "Combination", "process_std_dev": 1.0},
        {"type": "linear", "name": "Combination"},
        {"type": "max", "name": "Combination"},
        {"type": "mean", "name": "Combination"},
        {"type": "median", "name": "Combination"},
        {"type": "min", "name": "Combination"},
        {"type": "most_recently_updated", "name": "Combination"},
        {"type": "range", "name": "Combination"},
        {"type": "sum", "name": "Combination"},
        {"type": "weighted_mean", "name": "Combination"},
    ],
)
def test_combination_types_require_at_least_one_source(
    config: dict[str, str | float],
) -> None:
    with pytest.raises(cv.Invalid, match="at least 1"):
        CONFIG_SCHEMA({**config, "sources": []})


@pytest.mark.parametrize("weight", [0.0, 1.0, 1.401298464324817e-45, 3.4028235e38])
def test_weighted_mean_accepts_representable_constant_weights(weight: float) -> None:
    config = WEIGHTED_MEAN_SOURCE_SCHEMA({"source": "source", "weight": weight})
    assert config["weight"] == weight


@pytest.mark.parametrize("weight", [1e-50, 1e40, float("inf")])
def test_weighted_mean_rejects_unrepresentable_constant_weights(weight: float) -> None:
    with pytest.raises(cv.Invalid, match="C\\+\\+ float"):
        WEIGHTED_MEAN_SOURCE_SCHEMA({"source": "source", "weight": weight})


def test_weighted_mean_accepts_templated_weight() -> None:
    weight = Lambda("return 1.0f;")
    config = WEIGHTED_MEAN_SOURCE_SCHEMA({"source": "source", "weight": weight})
    assert config["weight"] is weight
