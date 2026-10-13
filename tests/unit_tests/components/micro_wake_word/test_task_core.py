"""Validate inference task affinity before generating firmware."""

import pytest

from esphome.components import audio, micro_wake_word as mww
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_ESP32, KEY_TARGET_PLATFORM, KEY_VARIANT
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.mark.parametrize(
    "variant",
    ["ESP32", "ESP32S3", "ESP32P4", "ESP32H4", "ESP32S31", "ESP32S2", "ESP32C3"],
)
@pytest.mark.parametrize("core", [-1, 0, 1])
def test_task_core_matches_target(variant: str, core: int) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    token = fv.full_config.set({"esp32": {"framework": {"sdkconfig_options": {}}}})
    try:
        config = mww.CONFIG_SCHEMA({"task_core": core})
        if core == 1 and variant in ("ESP32S2", "ESP32C3"):
            with pytest.raises(cv.Invalid, match="only available"):
                mww._validate_task_core(config)
        else:
            assert mww._validate_task_core(config)["task_core"] == core
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("core", [-2, 2])
def test_invalid_task_core(core: int) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    with pytest.raises(cv.Invalid):
        mww.CONFIG_SCHEMA({"task_core": core})


def test_default_task_core_is_unpinned() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    assert mww.CONFIG_SCHEMA({})["task_core"] == -1


@pytest.mark.parametrize("unicore", ["y", "n", "true", "1"])
def test_core_one_matches_freertos_unicore_setting(unicore: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
    token = fv.full_config.set(
        {
            "esp32": {
                "framework": {"sdkconfig_options": {"CONFIG_FREERTOS_UNICORE": unicore}}
            }
        }
    )
    try:
        if unicore == "y":
            with pytest.raises(cv.Invalid, match="dual-core"):
                mww._validate_task_core({"task_core": 1})
        else:
            assert mww._validate_task_core({"task_core": 1})["task_core"] == 1
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("variant", ["ESP32S3", "ESP32C3"])
def test_task_core_final_validation(
    variant: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    # Isolate microphone capabilities; exercise the real final-validation chain.
    monkeypatch.setattr(
        audio,
        "final_validate_audio_schema",
        lambda *args, **kwargs: lambda config: config,
    )
    token = fv.full_config.set({"esp32": {"framework": {"sdkconfig_options": {}}}})
    try:
        config = mww.CONFIG_SCHEMA({"task_core": 1})
        if variant == "ESP32C3":
            with pytest.raises(cv.Invalid, match="only available"):
                mww.FINAL_VALIDATE_SCHEMA(config)
        else:
            mww.FINAL_VALIDATE_SCHEMA(config)
    finally:
        fv.full_config.reset(token)
