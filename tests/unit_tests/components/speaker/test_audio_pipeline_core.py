"""Validate speaker media player task affinity against the target CPU."""

import pytest

from esphome.components.speaker import media_player as speaker_media_player
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_ESP32, KEY_TARGET_PLATFORM, KEY_VARIANT
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.mark.parametrize("variant", ["ESP32", "ESP32S3", "ESP32C3"])
@pytest.mark.parametrize("core", [None, 0, 1])
def test_audio_pipeline_core_matches_target(variant: str, core: int | None) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    token = fv.full_config.set({"esp32": {"framework": {"sdkconfig_options": {}}}})
    config = {} if core is None else {"audio_pipeline_core": core}
    try:
        if core == 1 and variant == "ESP32C3":
            with pytest.raises(cv.Invalid, match="only available"):
                speaker_media_player._validate_audio_pipeline_core(config)
        else:
            assert speaker_media_player._validate_audio_pipeline_core(config) == config
    finally:
        fv.full_config.reset(token)


def test_core_one_rejects_unicore_mode() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32"}
    token = fv.full_config.set(
        {
            "esp32": {
                "framework": {"sdkconfig_options": {"CONFIG_FREERTOS_UNICORE": "y"}}
            }
        }
    )
    try:
        with pytest.raises(cv.Invalid, match="dual-core"):
            speaker_media_player._validate_audio_pipeline_core(
                {"audio_pipeline_core": 1}
            )
    finally:
        fv.full_config.reset(token)
