"""Preset validation and code generation for the pid climate platform."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.pid.climate import _validate_default_preset, _validate_presets
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_PRESET
from esphome.types import ConfigType


def _presets(*names: str) -> list[ConfigType]:
    return [{CONF_NAME: name} for name in names]


@pytest.mark.parametrize("name", ["none", "NONE"])
def test_none_is_reserved(name: str) -> None:
    with pytest.raises(cv.Invalid, match="is reserved"):
        _validate_presets(_presets(name))


@pytest.mark.parametrize(
    "names", [("Home", "HOME"), ("Frost", "Frost"), ("Frost", "frost")]
)
def test_duplicate_names_are_rejected(names: tuple[str, str]) -> None:
    with pytest.raises(cv.Invalid, match="is defined twice"):
        _validate_presets(_presets(*names))


def test_distinct_names_are_accepted() -> None:
    presets = _presets("Home", "Sleep", "Frost protection")
    assert _validate_presets(presets) == presets


def test_default_preset_must_be_defined() -> None:
    config = {CONF_PRESET: _presets("Home"), "default_preset": "Sleep"}
    with pytest.raises(cv.Invalid, match="is not defined") as exc:
        _validate_default_preset(config)
    assert exc.value.path == ["default_preset"]


def test_default_preset_matches_standard_names_case_insensitively() -> None:
    config = {CONF_PRESET: _presets("Home"), "default_preset": "HOME"}
    assert _validate_default_preset(config) == config


def test_default_preset_matches_custom_names_exactly() -> None:
    config = {CONF_PRESET: _presets("Frost"), "default_preset": "frost"}
    with pytest.raises(cv.Invalid, match="is not defined"):
        _validate_default_preset(config)


def test_restore_from_default_preset_needs_a_default_preset() -> None:
    config = {CONF_PRESET: _presets("Home"), "on_boot_restore_from": "DEFAULT_PRESET"}
    with pytest.raises(cv.Invalid, match="needs a default_preset") as exc:
        _validate_default_preset(config)
    assert exc.value.path == ["on_boot_restore_from"]


def test_presets_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("presets.yaml"))

    assert "room->set_preset_config(pid_presets, 1);" in main_cpp
    assert "room->set_custom_preset_config(pid_custom_presets, 1);" in main_cpp
    assert "room->set_default_preset(climate::CLIMATE_PRESET_HOME);" in main_cpp
    assert (
        "room->set_on_boot_restore_from(pid::OnBootRestoreFrom::ON_BOOT_RESTORE_FROM_DEFAULT_PRESET);"
        in main_cpp
    )
    assert "room->add_on_preset_change_callback(" in main_cpp
