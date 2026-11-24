import pytest

from esphome import config_validation as cv
from esphome.components.combination.sensor import CONFIG_SCHEMA


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
