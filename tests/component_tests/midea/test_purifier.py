"""Tests for the optional Midea purifier switch."""

from collections.abc import Callable
from pathlib import Path
import shutil
import subprocess

import pytest

from esphome import config_validation as cv
from esphome.__main__ import generate_cpp_contents
from esphome.components.midea.climate import CONFIG_SCHEMA as CLIMATE_SCHEMA
from esphome.components.midea.switch import CONFIG_SCHEMA
from esphome.config import read_config, validate_config
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.writer import copy_src_tree
from esphome.yaml_util import load_yaml
from tests.component_tests.types import SetCoreConfigCallable


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


def test_codegen_uses_the_public_purifier_library(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Firmware dependencies resolve to the immutable public purifier implementation."""
    generate_main(component_config_path("purifier.yaml"))
    library = CORE.platformio_libraries["MideaUART"]
    assert library.as_lib_dep == (
        "MideaUART=https://github.com/m4rkireland/MideaUART.git#"
        "2e65f70ed12e507c69d9662b524d3027ac051960"
    )


@pytest.mark.parametrize("scenario", ["feedback", "accepted", "rejected"])
def test_feedback_is_the_only_source_of_switch_state(
    tmp_path: Path, scenario: str
) -> None:
    """Compile the real bridge against controlled framework/transport boundaries."""
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required for the Midea bridge host test")
    root = Path(__file__).resolve().parents[3]
    fixtures = Path(__file__).parent / "fixtures"
    sources = [
        fixtures / "purifier_test.cpp",
        root / "esphome/components/midea/purifier_switch.cpp",
    ]
    executable = tmp_path / "purifier_test"
    compiled = subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DESP_LOGW(...)=((void)0)",
            "-include",
            str(fixtures / "stubs/esphome/core/defines.h"),
            "-I",
            str(fixtures / "stubs"),
            "-I",
            str(root),
            *(str(source) for source in sources),
            "-o",
            str(executable),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert compiled.returncode == 0, compiled.stderr
    result = subprocess.run(
        [str(executable), scenario], capture_output=True, text=True, check=False
    )
    assert result.returncode == 0, result.stdout + result.stderr
