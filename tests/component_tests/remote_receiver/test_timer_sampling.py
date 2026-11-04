"""BK72xx timer sampling: option checks, defaults and the code it generates."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome import final_validate as fv
from esphome.components.libretiny.const import (
    FAMILY_BK7231Q,
    FAMILY_BK7231T,
    FAMILY_BK7238,
    KEY_BOARD,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
from esphome.components.remote_receiver import (
    CONF_SAMPLE_PERIOD,
    CONF_SAMPLING,
    SAMPLING_TIMER,
    final_validate_sampling,
    validate_sampling,
)
import esphome.config_validation as cv
from esphome.const import CONF_FILTER, CONF_IDLE
from esphome.core import CORE
from esphome.types import ConfigType


def _set_family(family: str) -> None:
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family, KEY_BOARD: "generic-board"}


def _us(value: int) -> cv.TimePeriodMicroseconds:
    return cv.positive_time_period_microseconds(f"{value}us")


def _timer(**options: object) -> ConfigType:
    return {CONF_SAMPLING: SAMPLING_TIMER, CONF_IDLE: _us(10000), **options}


def test_timer_sampling_sets_period_and_filter(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_bk72xx_timer.yaml"))
    assert "rcvr->set_sample_period_us(100);" in main_cpp
    # 1.5 sample periods by default
    assert "rcvr->set_filter_us(150);" in main_cpp


def test_edge_sampling_keeps_defaults(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_bk72xx.yaml"))
    assert "set_sample_period_us" not in main_cpp
    assert "rcvr->set_filter_us(50);" in main_cpp


def test_default_filter_is_capped_below_the_nec_mark() -> None:
    _set_family(FAMILY_BK7231T)
    config = validate_sampling(_timer(**{CONF_SAMPLE_PERIOD: _us(1000)}))
    assert config[CONF_FILTER].total_microseconds == 400


def test_sample_period_needs_timer_sampling() -> None:
    with pytest.raises(cv.Invalid, match="sample_period"):
        validate_sampling({CONF_SAMPLE_PERIOD: _us(100)})


def test_timer_sampling_rejected_on_bk7231q() -> None:
    """The BK7231Q SDK registers no timer driver."""
    _set_family(FAMILY_BK7231Q)
    with pytest.raises(cv.Invalid, match=FAMILY_BK7231Q):
        validate_sampling(_timer())


def test_idle_limited_with_timer_sampling() -> None:
    _set_family(FAMILY_BK7231T)
    with pytest.raises(cv.Invalid, match="idle"):
        validate_sampling(_timer(**{CONF_IDLE: _us(2000000)}))


def test_filter_warning_uses_default_sample_period(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A filter that cannot remove a one-sample glitch is reported."""
    _set_family(FAMILY_BK7231T)
    with caplog.at_level(logging.WARNING):
        validate_sampling(_timer(**{CONF_FILTER: _us(80)}))
    assert "sample period" in caplog.text


def test_default_filter_does_not_warn(caplog: pytest.LogCaptureFixture) -> None:
    _set_family(FAMILY_BK7231T)
    with caplog.at_level(logging.WARNING):
        validate_sampling(_timer())
    assert caplog.text == ""


@pytest.mark.parametrize(
    ("family", "full_config", "match"),
    [
        # remote_transmitter paces its envelope with BKTIMER1 on BK7238
        (
            FAMILY_BK7238,
            {"remote_receiver": [{}], "remote_transmitter": [{}]},
            "BKTIMER1",
        ),
        # one timer channel, one backend for the build
        (FAMILY_BK7231T, {"remote_receiver": [{}, {}]}, "only one"),
    ],
)
def test_final_validate_rejects(
    family: str, full_config: ConfigType, match: str
) -> None:
    _set_family(family)
    token = fv.full_config.set(full_config)
    try:
        with pytest.raises(cv.Invalid, match=match):
            final_validate_sampling({CONF_SAMPLING: SAMPLING_TIMER})
    finally:
        fv.full_config.reset(token)


_TIMER = {CONF_SAMPLING: SAMPLING_TIMER}


@pytest.mark.parametrize(
    ("family", "config", "full_config"),
    [
        # no transmitter on BK7238
        (FAMILY_BK7238, _TIMER, {"remote_receiver": [_TIMER]}),
        # other families pace the transmitter without BKTIMER1
        (
            FAMILY_BK7231T,
            _TIMER,
            {"remote_receiver": [_TIMER], "remote_transmitter": [{}]},
        ),
        # edge sampling never uses the timer
        (FAMILY_BK7238, {}, {"remote_receiver": [{}, {}], "remote_transmitter": [{}]}),
    ],
)
def test_final_validate_allows(
    family: str, config: ConfigType, full_config: ConfigType
) -> None:
    _set_family(family)
    token = fv.full_config.set(full_config)
    try:
        final_validate_sampling(config)
    finally:
        fv.full_config.reset(token)
