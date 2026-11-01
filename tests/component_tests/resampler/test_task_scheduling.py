"""Validate per-speaker task placement before generating firmware."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import audio
from esphome.components.resampler import speaker as resampler
import esphome.config_validation as cv
from esphome.const import KEY_VARIANT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_default_scheduling_is_unchanged(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: "ESP32S3"})
    config = resampler.CONFIG_SCHEMA({"output_speaker": "output"})
    assert config["task_core"] == -1
    assert config["task_priority"] == 1


@pytest.mark.parametrize(("core", "priority"), [(-1, 1), (0, 5), (1, 23)])
def test_scheduling_is_configurable(
    core: int, priority: int, set_core_config: SetCoreConfigCallable
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: "ESP32S3"})
    config = resampler.CONFIG_SCHEMA(
        {"output_speaker": "output", "task_core": core, "task_priority": priority}
    )
    assert (config["task_core"], config["task_priority"]) == (core, priority)


@pytest.mark.parametrize(
    "variant",
    [
        "ESP32",
        "ESP32S3",
        "ESP32P4",
        "ESP32H4",
        "ESP32S31",
        "ESP32S2",
        "ESP32C2",
        "ESP32C3",
        "ESP32C5",
        "ESP32C6",
        "ESP32H2",
    ],
)
@pytest.mark.parametrize("core", [-1, 0, 1])
def test_task_core_matches_target(
    variant: str, core: int, set_core_config: SetCoreConfigCallable
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={"esp32": {"framework": {"sdkconfig_options": {}}}},
    )
    config = resampler.CONFIG_SCHEMA({"output_speaker": "output", "task_core": core})
    if core == 1 and variant not in (
        "ESP32",
        "ESP32S3",
        "ESP32P4",
        "ESP32H4",
        "ESP32S31",
    ):
        with pytest.raises(cv.Invalid, match="only available"):
            resampler._validate_task_core(config)
    else:
        assert resampler._validate_task_core(config)["task_core"] == core


@pytest.mark.parametrize(
    ("key", "value"),
    [("task_core", -2), ("task_core", 2), ("task_priority", 0), ("task_priority", 24)],
)
def test_invalid_scheduling_values(
    key: str, value: int, set_core_config: SetCoreConfigCallable
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        resampler.CONFIG_SCHEMA({"output_speaker": "output", key: value})


@pytest.mark.parametrize("core", [-1, 0, 1])
@pytest.mark.parametrize("unicore", ["y", "n"])
def test_unicore_is_checked_in_final_validation(
    core: int,
    unicore: str,
    set_core_config: SetCoreConfigCallable,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: "ESP32S3"},
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
    if core == 1 and unicore == "y":
        with pytest.raises(cv.Invalid, match="dual-core"):
            resampler.FINAL_VALIDATE_SCHEMA(config)
    else:
        resampler.FINAL_VALIDATE_SCHEMA(config)


def test_non_esp32_target_remains_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid):
        resampler.CONFIG_SCHEMA({"output_speaker": "output"})


def test_scheduling_is_per_instance(generate_main: Callable[[str | Path], str]) -> None:
    cpp = generate_main(Path(__file__).parent / "config/task_scheduling.yaml")
    assert "default_resampler->set_task_scheduling<-1, 1>();" in cpp
    assert "core_zero_resampler->set_task_scheduling<0, 5>();" in cpp
    assert "core_one_resampler->set_task_scheduling<1, 23>();" in cpp
