"""KeeLoq suffix is a transmit key and has to fit in its width."""

from __future__ import annotations

import pytest

from esphome.components.remote_base import (
    CONF_SUFFIX,
    CONF_SUFFIX_BITS,
    KEELOQ_SCHEMA,
    _keeloq_suffix_fits,
)
import esphome.config_validation as cv


def test_binary_sensor_rejects_suffix() -> None:
    with pytest.raises(cv.Invalid):
        KEELOQ_SCHEMA(
            {"address": 0x1, "code": 0x2, CONF_SUFFIX: 1, CONF_SUFFIX_BITS: 8}
        )


def test_plain_suffix_must_fit() -> None:
    fitted = {"address": 0x1, "code": 0x2, CONF_SUFFIX: 0xA5, CONF_SUFFIX_BITS: 8}
    assert _keeloq_suffix_fits(fitted)[CONF_SUFFIX] == 0xA5
    with pytest.raises(cv.Invalid):
        _keeloq_suffix_fits({**fitted, CONF_SUFFIX: 0x100})


def test_lambda_suffix_is_not_checked_here() -> None:
    config = {"address": 0x1, "code": 0x2, CONF_SUFFIX: "id(x)", CONF_SUFFIX_BITS: 4}
    assert _keeloq_suffix_fits(config)[CONF_SUFFIX] == "id(x)"
