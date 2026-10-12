"""Tests for the optional Midea purifier switch."""

from collections.abc import Callable
from pathlib import Path
from urllib.parse import urlsplit

import pytest

from esphome import config_validation as cv
from esphome.__main__ import generate_cpp_contents
from esphome.components.midea.climate import CONFIG_SCHEMA as CLIMATE_SCHEMA
from esphome.components.midea.switch import CONFIG_SCHEMA
from esphome.config import read_config, validate_config
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.espidf.clang_tidy import _parse_lib_deps
from esphome.types import ConfigType
from esphome.writer import copy_src_tree
from esphome.yaml_util import load_yaml
from tests.component_tests.types import SetCoreConfigCallable


def _add_second_climate(config: ConfigType) -> None:
    config["uart"] = [
        {**config["uart"], "id": "first_uart"},
        {"id": "second_uart", "tx_pin": "GPIO4", "rx_pin": "GPIO5", "baud_rate": 9600},
    ]
    config["climate"][0]["uart_id"] = "first_uart"
    config["climate"].append(
        {
            "platform": "midea",
            "id": "second_climate",
            "name": "Second Climate",
            "uart_id": "second_uart",
        }
    )


def test_purifier_requires_explicit_switch_platform(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Climate configuration must not implicitly enable the switch platform."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="extra keys not allowed.*purifier"):
        CLIMATE_SCHEMA({"name": "Test Climate", "purifier": {"name": "Purifier"}})


def test_purifier_defaults(set_core_config: SetCoreConfigCallable) -> None:
    """An opt-in child disables restoration and requires recent feedback."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = CONFIG_SCHEMA(
        {"midea_id": "test_climate", "purifier": {"name": "Purifier"}}
    )
    assert config["purifier"]["name"] == "Purifier"
    assert str(config["purifier"]["restore_mode"]) == "DISABLED"
    assert config["purifier"]["status_max_age"].total_milliseconds == 15000


@pytest.mark.parametrize(
    "mode", ["ALWAYS_OFF", "ALWAYS_ON", "RESTORE_DEFAULT_OFF", "RESTORE_DEFAULT_ON"]
)
def test_purifier_rejects_restoration(
    set_core_config: SetCoreConfigCallable, mode: str
) -> None:
    """Only feedback may determine the initial switch state."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="DISABLED"):
        CONFIG_SCHEMA(
            {
                "midea_id": "test_climate",
                "purifier": {"name": "Purifier", "restore_mode": mode},
            }
        )


def test_purifier_has_a_bounded_status_age(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Freshness is finite and positive, including the rollover-safe upper bound."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    for age, milliseconds in (("1ms", 1), ("30s", 30000), ("2147483647ms", 0x7FFFFFFF)):
        config = CONFIG_SCHEMA(
            {
                "midea_id": "test_climate",
                "purifier": {"name": f"Purifier {age}", "status_max_age": age},
            }
        )
        assert config["purifier"]["status_max_age"].total_milliseconds == milliseconds
    for age in ("0ms", "-1s", "2147483648ms", "never"):
        with pytest.raises(cv.Invalid):
            CONFIG_SCHEMA(
                {
                    "midea_id": "test_climate",
                    "purifier": {"name": f"Purifier {age}", "status_max_age": age},
                }
            )


def test_purifier_rejects_inversion(set_core_config: SetCoreConfigCallable) -> None:
    """The reported ON/OFF state must retain the appliance's meaning."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="Inverted is not supported"):
        CONFIG_SCHEMA(
            {
                "midea_id": "test_climate",
                "purifier": {"name": "Purifier", "inverted": True},
            }
        )


@pytest.mark.parametrize("parent", ["omitted", "unknown", "wrong_type", "no_climate"])
def test_purifier_requires_a_midea_climate(
    component_config_path: Callable[[str], Path], parent: str
) -> None:
    """Full validation rejects absent and incorrectly typed parent references."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    if parent == "omitted":
        del config["switch"][0]["midea_id"]
        expected = "required key not provided"
    elif parent == "unknown":
        config["switch"][0]["midea_id"] = "missing_climate"
        expected = "Couldn't find ID 'missing_climate'"
    elif parent == "wrong_type":
        config["uart"]["id"] = "test_uart"
        config["switch"][0]["midea_id"] = "test_uart"
        expected = "doesn't inherit from midea::ac::AirConditioner"
    else:
        del config["climate"]
        expected = "requires component climate"
    result = validate_config(config, {})
    assert any(expected in str(error) for error in result.errors), result.errors


@pytest.mark.parametrize(
    ("period", "age", "valid"),
    [
        ("1s", "999ms", False),
        ("20s", None, False),
        ("1s", "1s", True),
        ("10s", "30s", True),
        ("1ms", "1ms", True),
        ("1s", "2147483647ms", True),
    ],
)
def test_purifier_status_age_covers_parent_period(
    component_config_path: Callable[[str], Path],
    period: str,
    age: str | None,
    valid: bool,
) -> None:
    """Full validation compares the feedback age with the referenced polling period."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    config["climate"][0]["period"] = period
    if age is None:
        del config["switch"][0]["purifier"]["status_max_age"]
    else:
        config["switch"][0]["purifier"]["status_max_age"] = age
    result = validate_config(config, {})
    if valid:
        assert not result.errors, result.errors
    else:
        assert any(
            "status_max_age must be at least" in str(error)
            and "response delays" in str(error)
            for error in result.errors
        ), result.errors


@pytest.mark.parametrize("parent", ["test_climate", "second_climate"])
def test_purifier_checks_only_its_referenced_period(
    component_config_path: Callable[[str], Path], parent: str
) -> None:
    """Multiple climates use separate UARTs and independent polling periods."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    _add_second_climate(config)
    config["climate"][0]["period"] = "60s"
    config["climate"][1]["period"] = "10s"
    config["switch"][0]["midea_id"] = parent
    config["switch"][0]["purifier"]["status_max_age"] = "15s"
    result = validate_config(config, {})
    if parent == "second_climate":
        assert not result.errors, result.errors
    else:
        assert len(result.errors) == 1
        assert "status_max_age must be at least" in str(result.errors[0])
        assert result.errors[0].path == ["switch", 0, "purifier", "status_max_age"]


@pytest.mark.parametrize(
    ("other_parent", "has_purifier", "valid"),
    [
        ("test_climate", True, False),
        ("second_climate", True, True),
        ("test_climate", False, True),
    ],
)
def test_purifier_has_one_switch_per_climate(
    component_config_path: Callable[[str], Path],
    other_parent: str,
    has_purifier: bool,
    valid: bool,
) -> None:
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    _add_second_climate(config)
    other: ConfigType = {"platform": "midea", "midea_id": other_parent}
    if has_purifier:
        other["purifier"] = {"id": "other_purifier", "name": "Other Purifier"}
    config["switch"].append(other)
    result = validate_config(config, {})
    if valid:
        assert not result.errors, result.errors
    else:
        assert any("Only one purifier" in str(error) for error in result.errors)
        assert result.errors[0].path == ["switch", 1, "midea_id"]


@pytest.mark.parametrize("configured", [True, False])
def test_switch_platform_copies_required_sources(
    component_config_path: Callable[[str], Path], tmp_path: Path, configured: bool
) -> None:
    """Real codegen and source copying enable the bridge only for an opt-in switch."""
    filename = "purifier.yaml" if configured else "no-purifier.yaml"
    CORE.config_path = component_config_path(filename)
    CORE.config = read_config({})
    assert CORE.config is not None
    generate_cpp_contents(CORE.config)
    main = CORE.cpp_global_section + CORE.cpp_main_section
    if configured:
        assert "App.register_switch(test_purifier," in main
        assert "test_climate->set_purifier_switch(test_purifier)" in main
        assert "test_climate, 30000" in main
    else:
        assert "PurifierSwitch" not in main
        assert "set_purifier_switch" not in main
        assert "switch" not in CORE.loaded_integrations
    CORE.build_path = tmp_path
    copy_src_tree()
    for filename in ("switch.h", "switch.cpp"):
        assert (
            tmp_path / "src/esphome/components/switch" / filename
        ).is_file() == configured
    defines = (tmp_path / "src/esphome/core/defines.h").read_text()
    assert ("#define USE_SWITCH" in defines) == configured
    assert ("#define USE_MIDEA_PURIFIER" in defines) == configured


@pytest.mark.parametrize("climate_count", [1, 2])
@pytest.mark.parametrize("empty_switch", [False, True])
def test_codegen_without_purifier_keeps_upstream_library(
    component_config_path: Callable[[str], Path],
    climate_count: int,
    empty_switch: bool,
) -> None:
    """Opt-out firmware retains the upstream library, including multiple owners."""
    CORE.config_path = component_config_path("no-purifier.yaml")
    config = load_yaml(CORE.config_path)
    if climate_count == 2:
        _add_second_climate(config)
    if empty_switch:
        config["switch"] = [{"platform": "midea", "midea_id": "test_climate"}]
    CORE.config = validate_config(config, {})
    assert not CORE.config.errors, CORE.config.errors
    generate_cpp_contents(CORE.config)
    repository = urlsplit(CORE.platformio_libraries["MideaUART"].repository)
    assert (repository.hostname, repository.path) == (
        "github.com",
        "/dudanov/MideaUART.git",
    )
    assert repository.fragment
    assert "USE_MIDEA_PURIFIER" not in {define.name for define in CORE.defines}


@pytest.mark.parametrize("framework", ["arduino", "espidf"])
@pytest.mark.parametrize("parent", ["test_climate", "second_climate"])
@pytest.mark.parametrize("switch_first", [False, True])
def test_codegen_uses_the_public_purifier_library(
    component_config_path: Callable[[str], Path],
    framework: str,
    parent: str,
    switch_first: bool,
) -> None:
    """All owners resolve one runtime/development dependency, regardless of order."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    del config["esp8266"]
    config["esp32"] = {
        "board": "nodemcu-32s",
        "framework": {"type": "arduino" if framework == "arduino" else "esp-idf"},
    }
    _add_second_climate(config)
    config["switch"][0]["midea_id"] = parent
    if switch_first:
        config["climate"] = config.pop("climate")
        config["climate"].reverse()
    CORE.config = validate_config(config, {})
    assert not CORE.config.errors, CORE.config.errors
    generate_cpp_contents(CORE.config)
    main = CORE.cpp_global_section + CORE.cpp_main_section
    assert f"{parent}->set_purifier_switch(test_purifier)" in main
    assert "USE_MIDEA_PURIFIER" in {define.name for define in CORE.defines}
    assert "test_purifier" not in {str(id_) for id_ in CORE.component_ids}
    assert ("WiFi" in CORE.platformio_libraries) == (framework == "arduino")
    library = CORE.platformio_libraries["MideaUART"]
    root = Path(__file__).resolve().parents[3]
    tidy_libraries = _parse_lib_deps(root / "platformio.ini", framework)
    assert library.repository in {
        dependency.repository for dependency in tidy_libraries
    }
