"""Tests for the speaker_source media player pipeline validation."""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import audio
from esphome.components.const import CONF_ANNOUNCEMENT_VOLUME, CONF_BITRATE
from esphome.components.speaker_source.media_player import (
    CONF_ANNOUNCEMENT_PIPELINE,
    CONF_MEDIA_PIPELINE,
    PIPELINE_SCHEMA,
    _validate_announcement_volume,
    _validate_pipeline,
)
from esphome.const import CONF_FORMAT, CONF_NUM_CHANNELS, CONF_SAMPLE_RATE

PIPELINE = {"speaker": "spk", "sources": ["src"]}


def test_announcement_volume_with_both_pipelines() -> None:
    config = {
        CONF_ANNOUNCEMENT_VOLUME: 0.6,
        CONF_ANNOUNCEMENT_PIPELINE: {},
        CONF_MEDIA_PIPELINE: {},
    }
    assert _validate_announcement_volume(config) is config


def test_no_announcement_volume_with_one_pipeline() -> None:
    config = {CONF_ANNOUNCEMENT_PIPELINE: {}}
    assert _validate_announcement_volume(config) is config


@pytest.mark.parametrize("pipeline", [CONF_ANNOUNCEMENT_PIPELINE, CONF_MEDIA_PIPELINE])
def test_announcement_volume_needs_both_pipelines(pipeline: str) -> None:
    with pytest.raises(cv.Invalid, match="requires both"):
        _validate_announcement_volume({CONF_ANNOUNCEMENT_VOLUME: 0.6, pipeline: {}})


@pytest.mark.parametrize("bitrate", [8, 48, 320])
def test_pipeline_accepts_mp3_bitrates(bitrate: int) -> None:
    config = PIPELINE_SCHEMA({**PIPELINE, CONF_FORMAT: "MP3", CONF_BITRATE: bitrate})
    assert config[CONF_BITRATE] == bitrate


def test_pipeline_without_bitrate() -> None:
    assert CONF_BITRATE not in PIPELINE_SCHEMA(PIPELINE)


@pytest.mark.parametrize("bitrate", [0, -48, 50, 48000])
def test_pipeline_rejects_bitrates_the_encoder_cannot_use(bitrate: int) -> None:
    with pytest.raises(cv.Invalid, match="Unknown value"):
        PIPELINE_SCHEMA({**PIPELINE, CONF_FORMAT: "MP3", CONF_BITRATE: bitrate})


def _final_validate(monkeypatch: pytest.MonkeyPatch, **overrides: Any) -> dict:
    """Run the pipeline's final validator with the speaker checks isolated."""
    monkeypatch.setattr(
        audio, "final_validate_audio_schema", lambda *args, **kwargs: lambda c: c
    )
    # Both audio properties are set, so nothing is inherited from the speaker.
    config = PIPELINE_SCHEMA(
        {**PIPELINE, CONF_SAMPLE_RATE: 48000, CONF_NUM_CHANNELS: 1, **overrides}
    )
    return _validate_pipeline(config)


def test_final_validation_keeps_mp3_bitrate(monkeypatch: pytest.MonkeyPatch) -> None:
    config = _final_validate(monkeypatch, **{CONF_FORMAT: "MP3", CONF_BITRATE: 48})
    assert config[CONF_BITRATE] == 48


def test_final_validation_without_bitrate(monkeypatch: pytest.MonkeyPatch) -> None:
    assert CONF_BITRATE not in _final_validate(monkeypatch, **{CONF_FORMAT: "FLAC"})


@pytest.mark.parametrize("format_", ["FLAC", "NONE", "WAV"])
def test_final_validation_rejects_bitrate_for_other_formats(
    format_: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    with pytest.raises(cv.Invalid, match="only supported for the MP3 format") as exc:
        _final_validate(monkeypatch, **{CONF_FORMAT: format_, CONF_BITRATE: 48})
    assert exc.value.path == [CONF_BITRATE]
