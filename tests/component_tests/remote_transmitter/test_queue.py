"""max_pending defaults to twice queue_depth and may not be smaller than it."""

import pytest

from esphome.components.remote_transmitter import (
    CONF_MAX_PENDING,
    CONF_QUEUE_DEPTH,
    _validate_queue,
)
import esphome.config_validation as cv


def test_max_pending_defaults_to_twice_queue_depth() -> None:
    config = {CONF_QUEUE_DEPTH: 4}
    _validate_queue(config)
    assert config[CONF_MAX_PENDING] == 8


def test_max_pending_explicit_is_kept() -> None:
    config = {CONF_QUEUE_DEPTH: 4, CONF_MAX_PENDING: 4}
    _validate_queue(config)
    assert config[CONF_MAX_PENDING] == 4


def test_max_pending_below_queue_depth_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="max_pending must be at least queue_depth"):
        _validate_queue({CONF_QUEUE_DEPTH: 4, CONF_MAX_PENDING: 2})


def test_no_queue_depth_without_rmt() -> None:
    config: dict[str, int] = {}
    _validate_queue(config)
    assert CONF_MAX_PENDING not in config
