import pytest

from esphome import config_validation as cv
from esphome.components.tmcm1240 import CONFIG_SCHEMA

BASE = {
    "node_id": 32,
    "profile_velocity": 1000,
    "acceleration": 2000,
    "deceleration": 2000,
    "run_current": 0.385,
    "standby_current": 0.1925,
    "stallguard_threshold": 8,
    "stallguard_min_velocity": 2800,
}


@pytest.mark.parametrize(
    "key,value",
    [
        ("run_current", 2.1),
        ("run_current", float("nan")),
        ("standby_current", float("nan")),
        ("run_current", 0.01),
        ("standby_current", 0.5),
        ("stallguard_threshold", -64),
        ("stallguard_threshold", 64),
    ],
)
def test_invalid_settings(key: str, value: object) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({**BASE, key: value})


def test_snapshot_currents() -> None:
    config = CONFIG_SCHEMA(BASE)
    assert config["run_current"] == 0.385
    assert config["standby_current"] == 0.1925
