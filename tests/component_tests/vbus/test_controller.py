"""Controller ownership, code generation and read-only configuration boundaries."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.components import vbus
import esphome.config_validation as cv
from esphome.core import CORE


def test_controller_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path("controller.yaml"))
    defines = {define.name for define in CORE.defines}
    assert {
        "USE_VBUS_CS_PLUS",
        "USE_VBUS_POLLING",
        "USE_VBUS_TRANSACTIONS",
        "USE_VBUS_TX",
    } <= defines
    assert "vbus::ResolDeltaSolCSPlus(controller_bus," in code
    assert "false)" in code
    assert "add_on_snapshot_callback(" in code
    assert "start_scan()" in code
    assert "suspend_polling()" in code
    assert "resume_polling()" in code
    assert "queue_write(" not in code
    assert vbus.CS_PLUS_SETTING_COUNT == 84


@pytest.mark.parametrize("interval", ["0s", "1s", "59999ms"])
def test_controller_rejects_excessive_refresh_frequency(interval: str) -> None:
    with pytest.raises(cv.Invalid, match="at least 60s"):
        vbus.CONTROLLER_SCHEMA(
            {"model": "deltasol_cs_plus", "update_interval": interval}
        )


@pytest.mark.parametrize("interval", ["60s", "5min", "never"])
def test_controller_accepts_bounded_and_manual_refresh(interval: str) -> None:
    assert vbus.CONTROLLER_SCHEMA(
        {"model": "deltasol_cs_plus", "update_interval": interval}
    )


def test_controller_defaults_and_supported_model() -> None:
    result = vbus.CONTROLLER_SCHEMA({"model": "deltasol_cs_plus"})
    assert result["update_interval"].total_milliseconds == 300000
    with pytest.raises(cv.Invalid):
        vbus.CONTROLLER_SCHEMA({"model": "deltasol_bs_plus"})


@pytest.mark.parametrize("field,value", [("destination", 0x4211), ("source", 0x0021)])
def test_controller_uses_only_the_qualified_addresses(field: str, value: int) -> None:
    settings = {
        "destination": 0x2211,
        "transactions": {"controller": {"model": "deltasol_cs_plus"}},
    }
    settings[field] = value
    with pytest.raises(cv.Invalid, match="CS Plus requires"):
        vbus.TRANSMIT_SCHEMA(settings)


def test_controller_and_generic_polling_are_exclusive() -> None:
    with pytest.raises(cv.Invalid):
        vbus.TRANSACTIONS_SCHEMA(
            {"polling": {"value_ids": [1]}, "controller": {"model": "deltasol_cs_plus"}}
        )


@pytest.mark.parametrize(
    "action", ["queue_read", "queue_write", "request_bus", "release_bus"]
)
def test_controller_rejects_raw_actions(
    action: str, tmp_path: Path, component_config_path: Callable[[str], Path]
) -> None:
    source = component_config_path("controller.yaml").read_text()
    source = (
        source[: source.index("button:")]
        + "button:\n  - platform: template\n    name: Raw\n    on_press:\n"
    )
    source += f"      - vbus.{action}:\n          id: controller_bus\n"
    if action in ["queue_read", "queue_write"]:
        source += "          value_id: 55\n"
    if action == "queue_write":
        source += "          value: 4\n"
    path = tmp_path / "invalid.yaml"
    path.write_text(source)
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    assert any("owns TX" in str(error) for error in result.errors), result.errors
    if action in ["request_bus", "release_bus"]:
        assert any("vbus.scan_controller" in str(error) for error in result.errors)
        assert not any("use queue_read" in str(error) for error in result.errors)


@pytest.mark.parametrize(
    "fixture", ["rx_only.yaml", "transmit.yaml", "transactions.yaml", "polling.yaml"]
)
def test_controller_is_not_included_in_existing_configurations(
    fixture: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    code = generate_main(component_config_path(fixture))
    assert all(define.name != "USE_VBUS_CS_PLUS" for define in CORE.defines)
    assert "ResolDeltaSolCSPlus" not in code
