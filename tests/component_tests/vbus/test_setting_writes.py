"""Writing is an explicit controller opt-in, separate from raw P2 actions."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
from esphome.core import CORE


def test_setting_write_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("writes.yaml"))
    assert "USE_VBUS_CS_PLUS_WRITE" in {define.name for define in CORE.defines}
    assert "set_require_write_arm(false)" in code
    assert "request_setting(55, 4.0f)" in code
    assert "arm_writes(true)" in code
    assert "add_on_setting_result_callback(" in code


def test_writes_require_one_shot_arming_by_default() -> None:
    result = vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus", "writes": {}})
    assert result["writes"]["require_write_arm"] is True


@pytest.mark.parametrize("action", ["set_setting", "arm_writes"])
def test_setting_actions_require_explicit_writes(
    action: str,
    tmp_path: Path,
    component_config_path: Callable[[str], Path],
) -> None:
    source = component_config_path("controller.yaml").read_text().split("button:")[0]
    source += f"button:\n  - platform: template\n    name: Setting\n    on_press:\n      - vbus.{action}:\n          id: solar_controller\n"
    if action == "set_setting":
        source += "          value_id: 55\n          value: 4\n"
    else:
        source += "          enabled: true\n"
    path = tmp_path / "readonly.yaml"
    path.write_text(source)
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("controller.writes" in str(error) for error in result.errors), (
        result.errors
    )


@pytest.mark.parametrize("fixture", ["controller.yaml", "polling.yaml", "rx_only.yaml"])
def test_setting_write_code_is_absent_without_the_opt_in(
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    assert "USE_VBUS_CS_PLUS_WRITE" not in {define.name for define in CORE.defines}
