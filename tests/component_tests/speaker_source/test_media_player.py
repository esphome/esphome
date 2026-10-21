"""Tests for the speaker_source media player announcement volume validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_ANNOUNCEMENT_VOLUME
from esphome.components.speaker_source.media_player import (
    CONF_ANNOUNCEMENT_PIPELINE,
    CONF_MEDIA_PIPELINE,
    _validate_announcement_volume,
)


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
