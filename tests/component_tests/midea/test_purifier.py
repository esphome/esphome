"""Tests for the optional Midea purifier switch."""

from collections.abc import Callable
from pathlib import Path
from urllib.parse import urlsplit

import pytest

from esphome import config_validation as cv
from esphome.components.midea.switch import CONFIG_SCHEMA
from esphome.config import validate_config
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.espidf.clang_tidy import _parse_lib_deps
from esphome.yaml_util import load_yaml
from tests.component_tests.types import SetCoreConfigCallable


def test_purifier_defaults(set_core_config: SetCoreConfigCallable) -> None:
    """Default to recent feedback without restoring state."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    config = CONFIG_SCHEMA(
        {"midea_id": "test_climate", "purifier": {"name": "Purifier"}}
    )
    assert str(config["purifier"]["restore_mode"]) == "DISABLED"
    assert config["purifier"]["status_max_age"].total_milliseconds == 15000


@pytest.mark.parametrize(
    "options",
    [{"restore_mode": "ALWAYS_ON"}, {"inverted": True}, {"status_max_age": "never"}],
)
def test_purifier_rejects_unsafe_options(
    set_core_config: SetCoreConfigCallable, options: dict[str, str | bool]
) -> None:
    """Reject restoration, inversion and unbounded feedback age."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {"midea_id": "test_climate", "purifier": {"name": "Purifier", **options}}
        )


def test_purifier_requires_a_midea_climate(
    component_config_path: Callable[[str], Path],
) -> None:
    """Reject an unknown parent."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    config["switch"][0]["midea_id"] = "missing_climate"
    result = validate_config(config, {})
    assert any("Couldn't find ID 'missing_climate'" in str(e) for e in result.errors)


def test_purifier_status_age_covers_parent_period(
    component_config_path: Callable[[str], Path],
) -> None:
    """Reject feedback that expires before the next poll."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    config["climate"][0]["period"] = "60s"
    result = validate_config(config, {})
    assert any("status_max_age must be at least" in str(e) for e in result.errors)


def test_purifier_has_one_switch_per_climate(
    component_config_path: Callable[[str], Path],
) -> None:
    """Reject two switches controlling the same purifier."""
    CORE.config_path = component_config_path("purifier.yaml")
    config = load_yaml(CORE.config_path)
    config["switch"].append(
        {
            "platform": "midea",
            "midea_id": "test_climate",
            "purifier": {"name": "Other Purifier"},
        }
    )
    result = validate_config(config, {})
    assert any("Only one purifier" in str(e) for e in result.errors)


@pytest.mark.parametrize("configured", [True, False])
def test_purifier_codegen(
    component_config_path: Callable[[str], Path],
    generate_main: Callable[[str | Path], str],
    configured: bool,
) -> None:
    """Only an opt-in switch enables the bridge and its library."""
    filename = "purifier.yaml" if configured else "no-purifier.yaml"
    main = generate_main(component_config_path(filename))
    defines = {define.name for define in CORE.defines}
    assert ("USE_MIDEA_PURIFIER" in defines) == configured
    library = CORE.platformio_libraries["MideaUART"].repository
    if configured:
        assert "App.register_switch(test_purifier," in main
        assert "test_climate->set_purifier_switch(test_purifier)" in main
        assert "test_climate, 30000" in main
        assert "test_purifier" not in {str(id_) for id_ in CORE.component_ids}
        root = Path(__file__).resolve().parents[3]
        assert library in {
            dependency.repository
            for dependency in _parse_lib_deps(root / "platformio.ini", "arduino")
        }
    else:
        assert "PurifierSwitch" not in main
        repository = urlsplit(library)
        assert (repository.hostname, repository.path) == (
            "github.com",
            "/dudanov/MideaUART.git",
        )
        assert repository.fragment
        assert "switch" not in CORE.loaded_integrations
