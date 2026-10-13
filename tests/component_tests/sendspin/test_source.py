"""Validation and code generation tests for the sendspin source role.

These cover the rejection branches of the source block and the generated role
config, which a compile test cannot check: a `test*.yaml` can only assert that a
configuration is accepted.
"""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.automation import ACTION_REGISTRY, CONDITION_REGISTRY
from esphome.components.audio import CONF_MAX_SAMPLE_RATE, CONF_MIN_SAMPLE_RATE
from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.components.sendspin import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA, _get_data
from esphome.config import Config
from esphome.const import CONF_ID, CONF_SOURCE, PlatformFramework
from esphome.core import CORE, ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "sendspin_hub"
MIC_ID = "mic"


def _full_config(min_sample_rate: int = 16000, max_sample_rate: int = 16000) -> Config:
    """A full config carrying one microphone, as the ID pass leaves it."""
    mic: ConfigType = {
        CONF_ID: ID(MIC_ID, is_declaration=True, type="microphone"),
        CONF_MIN_SAMPLE_RATE: min_sample_rate,
        CONF_MAX_SAMPLE_RATE: max_sample_rate,
        "min_channels": 1,
        "max_channels": 2,
        "min_bits_per_sample": 16,
        "max_bits_per_sample": 32,
    }
    full = Config()
    full["microphone"] = [mic]
    full.declare_ids.append((mic[CONF_ID], ["microphone", 0, CONF_ID]))
    return full


def _hub_config(**source: Any) -> ConfigType:
    """Validate a hub config with a source block, allowing field overrides."""
    source_config: ConfigType = {
        "microphone": {"microphone": ID(MIC_ID, is_declaration=False)}
    }
    source_config.update(source)
    return CONFIG_SCHEMA({CONF_ID: HUB_ID, CONF_SOURCE: source_config})


@pytest.fixture
def microphone(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())


@pytest.mark.usefixtures("microphone")
def test_final_validate_records_sample_rate() -> None:
    """The stream takes the microphone's own rate, since nothing can resample it."""
    FINAL_VALIDATE_SCHEMA(_hub_config())

    assert _get_data().source_sample_rate == 16000


@pytest.mark.usefixtures("microphone")
def test_unsupported_bits_per_sample_rejected() -> None:
    with pytest.raises(cv.Invalid, match="bits_per_sample must be one of"):
        _hub_config(microphone={"microphone": MIC_ID, "bits_per_sample": 20})


def test_variable_sample_rate_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=_full_config(min_sample_rate=16000, max_sample_rate=48000),
    )

    with pytest.raises(cv.Invalid, match="must have a fixed sample rate"):
        FINAL_VALIDATE_SCHEMA(_hub_config())


def test_microphone_source_errors_point_at_the_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A channel the microphone does not have is reported at source: microphone:."""
    full_config = _full_config()
    full_config["microphone"][0]["max_channels"] = 1
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    with pytest.raises(cv.Invalid) as err:
        FINAL_VALIDATE_SCHEMA(
            _hub_config(microphone={"microphone": MIC_ID, "channels": [0, 1]})
        )
    assert err.value.path[:2] == [CONF_SOURCE, "microphone"]


@pytest.mark.parametrize(
    ("registry", "name", "action_config"),
    [
        (ACTION_REGISTRY, "sendspin.source.set_signal", {"present": True}),
        (CONDITION_REGISTRY, "sendspin.source.is_streaming", {}),
    ],
)
def test_source_automation_without_source_rejected(
    set_core_config: SetCoreConfigCallable,
    registry: dict[str, Any],
    name: str,
    action_config: ConfigType,
) -> None:
    """The source action and condition have nothing to act on without a source block."""
    set_core_config(PlatformFramework.ESP32_IDF)
    registry[name].schema({CONF_ID: ID(HUB_ID, is_declaration=False), **action_config})

    with pytest.raises(cv.Invalid, match="require a 'source' block"):
        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA({CONF_ID: HUB_ID}))


def _sdkconfig_option(name: str) -> Any:
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS].get(name)


def test_line_sense_source_config_generated(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The set_signal action turns on line sense."""
    main_cpp = generate_main(component_config_path("source_line_sense.yaml"))

    assert "set_source_config(" in main_cpp
    assert ".sample_rate = 48000" in main_cpp
    assert ".line_sense = true" in main_cpp
    assert _sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE") is None
    assert _sdkconfig_option("CONFIG_SENDSPIN_ENABLE_OPUS") is False


def test_default_source_config_generated(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Fields with no option keep sendspin-cpp's defaults, and is_streaming alone leaves line sense off."""
    main_cpp = generate_main(component_config_path("source_defaults.yaml"))

    assert ".sample_rate = 16000" in main_cpp
    assert ".chunk_duration_ms" not in main_cpp
    assert ".capture_buffer_ms" not in main_cpp
    assert ".line_sense = false" in main_cpp
    assert ".buffer_location" not in main_cpp
    assert _sdkconfig_option("CONFIG_SENDSPIN_ENABLE_OPUS") is False


def test_no_source_disables_the_role(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("device_info_default.yaml"))

    assert "set_source_config(" not in main_cpp
    assert _sdkconfig_option("CONFIG_SENDSPIN_ENABLE_SOURCE") is False
