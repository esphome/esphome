"""Validate per-speaker task placement before generating firmware."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import audio
from esphome.components.resampler import speaker as resampler
import esphome.config_validation as cv
from esphome.const import KEY_VARIANT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("core", [-2, 2])
def test_invalid_task_core(core: int, set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        resampler.CONFIG_SCHEMA({"output_speaker": "output", "task_core": core})


@pytest.mark.parametrize("core", [-1, 0, 1])
@pytest.mark.parametrize(
    ("variant", "unicore"), [("ESP32S3", "n"), ("ESP32S3", "y"), ("ESP32C3", "n")]
)
def test_task_core_final_validation(
    core: int,
    variant: str,
    unicore: str,
    set_core_config: SetCoreConfigCallable,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={
            "esp32": {
                "framework": {"sdkconfig_options": {"CONFIG_FREERTOS_UNICORE": unicore}}
            }
        },
    )
    # Isolate the output's audio capabilities, keeping the real final-validation chain.
    monkeypatch.setattr(
        resampler, "inherit_property_from", lambda *args: lambda config: config
    )
    monkeypatch.setattr(
        audio,
        "final_validate_audio_schema",
        lambda *args, **kwargs: lambda config: config,
    )
    config = resampler.CONFIG_SCHEMA({"output_speaker": "output", "task_core": core})
    if core == 1 and (variant == "ESP32C3" or unicore == "y"):
        with pytest.raises(cv.Invalid, match="only available|dual-core"):
            resampler.FINAL_VALIDATE_SCHEMA(config)
    else:
        resampler.FINAL_VALIDATE_SCHEMA(config)


def test_non_esp32_target_remains_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid):
        resampler.CONFIG_SCHEMA({"output_speaker": "output"})


def test_task_core_is_per_instance(generate_main: Callable[[str | Path], str]) -> None:
    cpp = generate_main(Path(__file__).parent / "config/task_core.yaml")
    assert "default_resampler->set_task_core<-1>();" in cpp
    assert "core_zero_resampler->set_task_core<0>();" in cpp
    assert "core_one_resampler->set_task_core<1>();" in cpp
