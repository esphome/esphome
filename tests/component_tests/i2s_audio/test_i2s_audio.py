"""Tests for the i2s_audio full duplex bus."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.i2s_audio import _validate_full_duplex
from esphome.core import CORE, ID

BUS_ID = "duplex_bus"


def _device(**overrides: Any) -> dict[str, Any]:
    return {
        "platform": "i2s_audio",
        "id": ID("device", is_declaration=True),
        "i2s_audio_id": ID(BUS_ID, is_declaration=False),
        "i2s_dout_pin": 17,
        "channel": "stereo",
        "i2s_comm_fmt": "stand_i2s",
        "sample_rate": 48000,
        "bits_per_sample": 32,
        "i2s_mode": "primary",
        "use_apll": False,
        "mclk_multiple": 256,
    } | overrides


def _full_config(
    microphones: list[dict[str, Any]], speakers: list[dict[str, Any]]
) -> dict[str, Any]:
    return {"microphone": microphones, "speaker": speakers}


def test_full_duplex_generates_code(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Both devices are registered with the bus and the full duplex code is enabled."""
    main_cpp = generate_main("tests/component_tests/i2s_audio/test_full_duplex.yaml")

    assert "duplex_bus->set_audio_in(duplex_mic);" in main_cpp
    assert "duplex_bus->set_audio_out(duplex_speaker);" in main_cpp
    assert "USE_I2S_AUDIO_FULL_DUPLEX" in {define.name for define in CORE.defines}


def test_full_duplex_generates_code_for_multiple_speakers(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Every speaker on the bus is registered; they share one TX configuration and take turns using it."""
    main_cpp = generate_main(
        "tests/component_tests/i2s_audio/test_full_duplex_multiple_speakers.yaml"
    )

    assert "duplex_bus->set_audio_in(duplex_mic);" in main_cpp
    assert "duplex_bus->set_audio_out(duplex_speaker_a);" in main_cpp
    assert "duplex_bus->set_audio_out(duplex_speaker_b);" in main_cpp


def test_full_duplex_accepts_matching_devices() -> None:
    """A microphone and speaker with the same clock settings are accepted."""
    _validate_full_duplex(
        _full_config([_device(pdm=False)], [_device(spdif_mode=False)]), BUS_ID
    )


def test_full_duplex_ignores_devices_on_other_buses() -> None:
    """Devices on another bus do not count towards the full duplex pair."""
    other = _device(i2s_audio_id=ID("other_bus", is_declaration=False))
    _validate_full_duplex(_full_config([_device(), other], [_device(), other]), BUS_ID)


@pytest.mark.parametrize(
    ("microphones", "speakers"),
    [
        ([], [_device()]),
        ([_device()], []),
        ([_device(), _device()], [_device()]),
    ],
)
def test_full_duplex_requires_one_microphone_and_a_speaker(
    microphones: list[dict[str, Any]], speakers: list[dict[str, Any]]
) -> None:
    """Exactly one microphone and at least one speaker must use the bus."""
    with pytest.raises(cv.Invalid, match="exactly one i2s_audio microphone"):
        _validate_full_duplex(_full_config(microphones, speakers), BUS_ID)


def test_full_duplex_rejects_pdm_microphone() -> None:
    """PDM receive mode cannot share the standard mode clocks."""
    with pytest.raises(cv.Invalid, match="PDM microphone"):
        _validate_full_duplex(_full_config([_device(pdm=True)], [_device()]), BUS_ID)


def test_full_duplex_rejects_spdif_speaker() -> None:
    """An SPDIF speaker cannot share the bus."""
    with pytest.raises(cv.Invalid, match="SPDIF speaker"):
        _validate_full_duplex(
            _full_config([_device()], [_device(spdif_mode=True)]), BUS_ID
        )


@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("sample_rate", 16000),
        ("bits_per_sample", 16),
        ("i2s_mode", "secondary"),
        ("use_apll", True),
        ("mclk_multiple", 384),
    ],
)
def test_full_duplex_rejects_mismatched_clock_settings(key: str, value: Any) -> None:
    """Settings that shape the shared clocks must match."""
    with pytest.raises(cv.Invalid, match=f"must use the same '{key}'"):
        _validate_full_duplex(
            _full_config([_device()], [_device(**{key: value})]), BUS_ID
        )


def test_full_duplex_accepts_multiple_matching_speakers() -> None:
    """Speakers that share the TX configuration can take turns on the bus."""
    _validate_full_duplex(
        _full_config([_device()], [_device(), _device(), _device()]), BUS_ID
    )


@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("i2s_dout_pin", 18),
        ("channel", "mono"),
        ("i2s_comm_fmt", "stand_msb"),
    ],
)
def test_full_duplex_rejects_mismatched_speakers(key: str, value: Any) -> None:
    """Settings that shape the TX channel must match across speakers."""
    with pytest.raises(
        cv.Invalid,
        match=f"speakers on 'full_duplex' bus '{BUS_ID}' must use the same '{key}'",
    ):
        _validate_full_duplex(
            _full_config([_device()], [_device(), _device(**{key: value})]), BUS_ID
        )
