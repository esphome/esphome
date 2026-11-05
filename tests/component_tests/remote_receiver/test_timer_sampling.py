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
    timer_sampling_defaults,
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


def _timer(
    filter_us: int = 150, sample_us: int = 100, idle_us: int = 10000
) -> ConfigType:
    """A timer config as the schema hands it to validate_sampling."""
    return {
        CONF_SAMPLING: SAMPLING_TIMER,
        CONF_SAMPLE_PERIOD: _us(sample_us),
        CONF_FILTER: _us(filter_us),
        CONF_IDLE: _us(idle_us),
    }


def test_timer_sampling_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_bk72xx_timer.yaml"))
    assert "rcvr->set_sample_period_us(100);" in main_cpp
    # 1.5 sample periods by default
    assert "rcvr->set_filter_us(150);" in main_cpp
    # the default 4000 byte buffer becomes two static halves of 500 pulses
    assert "static int32_t rcvr__frames[1000];" in main_cpp
    assert "rcvr->set_frame_buffer(rcvr__frames, 500);" in main_cpp
    assert "set_buffer_size" not in main_cpp


def test_timer_frame_buffer_avoids_config_ids(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(
        component_config_path("receiver_bk72xx_timer_id_clash.yaml")
    )
    assert "static int32_t rcvr__frames_2[1000];" in main_cpp
    assert "rcvr->set_frame_buffer(rcvr__frames_2, 500);" in main_cpp


def test_edge_sampling_keeps_defaults(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("receiver_bk72xx.yaml"))
    assert "set_sample_period_us" not in main_cpp
    assert "__frames" not in main_cpp
    assert "rcvr->set_filter_us(50);" in main_cpp


def test_default_filter_is_capped_below_the_nec_mark() -> None:
    config = timer_sampling_defaults(
        {CONF_SAMPLING: SAMPLING_TIMER, CONF_SAMPLE_PERIOD: "1000us"}
    )
    assert config[CONF_FILTER] == "400us"


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
        validate_sampling(_timer(idle_us=2000000))


@pytest.mark.parametrize(
    ("filter_us", "sample_us", "warns"),
    [
        # cannot remove a one-sample glitch
        (80, 100, True),
        # 1.5 sample periods removes a one-sample glitch
        (150, 100, False),
        # the default is capped below a long sample period, so it cannot remove one either
        (400, 1000, True),
    ],
)
def test_filter_warning(
    caplog: pytest.LogCaptureFixture, filter_us: int, sample_us: int, warns: bool
) -> None:
    _set_family(FAMILY_BK7231T)
    with caplog.at_level(logging.WARNING):
        validate_sampling(_timer(filter_us, sample_us))
    assert ("sample period" in caplog.text) is warns


@pytest.mark.parametrize(
    ("family", "config", "full_config", "match"),
    [
        # remote_transmitter paces its envelope with BKTIMER1 on BK7238
        (
            FAMILY_BK7238,
            _timer(),
            {"remote_receiver": [_timer()], "remote_transmitter": [{}]},
            "BKTIMER1",
        ),
        # one timer channel, one backend for the build
        (
            FAMILY_BK7231T,
            _timer(),
            {"remote_receiver": [_timer(), {}]},
            "every remote_receiver",
        ),
        # no transmitter on BK7238
        (FAMILY_BK7238, _timer(), {"remote_receiver": [_timer()]}, None),
        # other families pace the transmitter without BKTIMER1
        (
            FAMILY_BK7231T,
            _timer(),
            {"remote_receiver": [_timer()], "remote_transmitter": [{}]},
            None,
        ),
        # edge sampling never uses the timer
        (
            FAMILY_BK7238,
            {},
            {"remote_receiver": [{}, {}], "remote_transmitter": [{}]},
            None,
        ),
    ],
)
def test_final_validate(
    family: str, config: ConfigType, full_config: ConfigType, match: str | None
) -> None:
    _set_family(family)
    token = fv.full_config.set(full_config)
    try:
        if match is None:
            assert final_validate_sampling(config) is config
        else:
            with pytest.raises(cv.Invalid, match=match):
                final_validate_sampling(config)
    finally:
        fv.full_config.reset(token)
