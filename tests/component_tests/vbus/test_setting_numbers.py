"""Qualified numeric controls use confirmed controller snapshots and explicit write access."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components.vbus.cs_plus_numbers import NUMBER_PARAMETERS
from esphome.core import CORE


def test_all_58_number_parameters_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("numbers.yaml"))
    assert len(NUMBER_PARAMETERS) == 58
    assert len({spec.index for spec in NUMBER_PARAMETERS.values()}) == 58
    assert code.count("vbus::VBusSettingNumber(number_controller,") == 58
    assert "USE_VBUS_CS_PLUS_ENTITIES" in {define.name for define in CORE.defines}
    assert "USE_VBUS_CS_PLUS_WRITE" in {define.name for define in CORE.defines}
    assert "number_controller, 48)" in code
    assert "number_controller, 385)" in code
    assert "set_step(0.5f)" in code
    assert code.count("number_controller->register_setting_listener(") == 58
    assert "App.register_component_(vbus_vbussettingnumber" not in code


@pytest.mark.parametrize("parameter", ["display_language", "unknown_setting"])
def test_number_rejects_selects_and_unknown_parameters(
    parameter: str,
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    source = component_config_path("numbers.yaml").read_text()
    source = source.replace("parameter: system_time", f"parameter: {parameter}", 1)
    path = tmp_path / "invalid.yaml"
    path.write_text(source)
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert result.errors
    assert any("parameter" in str(error) for error in result.errors)


def test_number_requires_writable_controller(
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    source = component_config_path("numbers.yaml").read_text()
    source = source.replace("        writes:\n          require_write_arm: false\n", "")
    path = tmp_path / "readonly.yaml"
    path.write_text(source)
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("controller.writes" in str(error) for error in result.errors), (
        result.errors
    )


@pytest.mark.parametrize("fixture", ["writes.yaml", "controller.yaml", "rx_only.yaml"])
def test_entity_storage_is_not_included_without_setting_entities(
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    assert "USE_VBUS_CS_PLUS_ENTITIES" not in {define.name for define in CORE.defines}
