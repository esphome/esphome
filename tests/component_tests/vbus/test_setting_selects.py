"""Enumerated controls allow only the qualified option values and unique labels."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components.vbus.cs_plus_selects import SELECT_PARAMETERS
from esphome.core import CORE


def test_all_26_select_parameters_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("selects.yaml"))
    assert len(SELECT_PARAMETERS) == 26
    assert len({spec.index for spec in SELECT_PARAMETERS.values()}) == 26
    assert code.count("vbus::VBusSettingSelect(select_controller,") == 26
    assert code.count("select_controller->register_setting_listener(") == 26
    assert code.count("->initialize_options()") == 26
    assert "App.register_component_(vbus_vbussettingselect" not in code
    assert '"Italian"' in code


def test_all_84_controls_share_one_sufficiently_sized_controller_listener_list(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    source = component_config_path("selects.yaml").read_text()
    numbers = component_config_path("numbers.yaml").read_text().split("number:\n", 1)[1]
    source += "number:\n" + numbers.replace("number_controller", "select_controller")
    path = tmp_path / "all-controls.yaml"
    path.write_text(source)
    code = generate_main(path)
    assert code.count("select_controller->register_setting_listener(") == 84
    count = next(
        define for define in CORE.defines if define.name == "VBUS_SETTING_ENTITY_COUNT"
    )
    assert str(count.value) == "84"


@pytest.mark.parametrize("parameter", ["system_time", "unknown_setting"])
def test_select_rejects_numbers_and_unknown_parameters(
    parameter: str,
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    source = component_config_path("selects.yaml").read_text()
    source = source.replace("parameter: system_scheme", f"parameter: {parameter}", 1)
    path = tmp_path / "invalid.yaml"
    path.write_text(source)
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("parameter" in str(error) for error in result.errors)


@pytest.mark.parametrize(
    "options",
    [
        {1: "One"},
        {**dict.fromkeys(range(1, 11), "Same")},
        {**{i: str(i) for i in range(1, 11)}, 11: "Extra"},
        {**{i: str(i) for i in range(1, 11)}, 1: ""},
        {**{i: str(i) for i in range(1, 11)}, 1: "One\0invalid"},
    ],
)
def test_select_rejects_partial_extra_duplicate_or_empty_labels(
    options: dict[int, str],
    component_config_path: Callable[[str], Path],
) -> None:
    path = component_config_path("selects.yaml")
    CORE.config_path = path
    source = yaml_util.load_yaml(path)
    source["select"][0]["options"] = options
    result = config.validate_config(source, {})
    assert result.errors


def test_select_requires_explicit_controller_write_access(
    component_config_path: Callable[[str], Path],
) -> None:
    path = component_config_path("selects.yaml")
    CORE.config_path = path
    source = yaml_util.load_yaml(path)
    del source["vbus"]["transmit"]["transactions"]["controller"]["writes"]
    result = config.validate_config(source, {})
    assert any("controller.writes" in str(error) for error in result.errors)


def test_select_accepts_unique_custom_labels_without_changing_raw_values(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    tmp_path: Path,
) -> None:
    source = component_config_path("selects.yaml").read_text()
    labels = "\n".join(f'      {i}: "Schema {i}"' for i in range(1, 11))
    source = source.replace(
        "parameter: system_scheme",
        f"parameter: system_scheme\n    options:\n{labels}",
        1,
    )
    path = tmp_path / "custom.yaml"
    path.write_text(source)
    code = generate_main(path)
    assert '"Schema 10"' in code
    assert "select_controller, 1, 1)" in code
