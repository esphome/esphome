"""Tests for the http_request watchdog timeout default."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import load_config, read_config
from esphome.const import CONF_WATCHDOG_TIMEOUT
from esphome.core import CORE, TimePeriodMilliseconds


@pytest.mark.parametrize(
    ("yaml_file", "expected_ms"),
    [
        # stock 4.5s timeout: 3 x 4.5s plus 1s margin
        ("test_esp32_stock.yaml", 14500),
        # 3 x 10s plus 1s margin
        ("test_esp32_default.yaml", 31000),
        # esp32.watchdog_timeout: 60s is wider than the derived value and wins
        ("test_esp32_platform_wider.yaml", 60000),
        # explicit value is kept as is
        ("test_esp32_explicit.yaml", 20000),
    ],
)
def test_esp32_watchdog_timeout(
    component_config_path: Callable[[str], Path], yaml_file: str, expected_ms: int
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    assert config["http_request"][CONF_WATCHDOG_TIMEOUT] == TimePeriodMilliseconds(
        milliseconds=expected_ms
    )


@pytest.mark.parametrize("yaml_file", ["test_esp8266.yaml", "test_rp2040.yaml"])
def test_other_platforms_leave_watchdog_unset(
    component_config_path: Callable[[str], Path], yaml_file: str
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    config = read_config({})
    assert CONF_WATCHDOG_TIMEOUT not in config["http_request"]


@pytest.mark.parametrize(
    ("yaml_file", "target"),
    [
        ("update_ota_default.yaml", "http_request_otahttprequestcomponent_id"),
        ("update_target_explicit.yaml", "device_ota"),
        ("update_deprecated_ota_id.yaml", "device_ota"),
        ("update_target.yaml", "flasher"),
    ],
)
def test_update_installs_through_its_target(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    yaml_file: str,
    target: str,
) -> None:
    """Any UpdateTarget installs the update, the device OTA component included."""
    main_cpp = generate_main(component_config_path(yaml_file))
    assert f"set_target({target})" in main_cpp
    assert "USE_HTTP_REQUEST_UPDATE" in {define.name for define in CORE.defines}


def test_deprecated_ota_id_warns(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    generate_main(component_config_path("update_deprecated_ota_id.yaml"))
    assert "'ota_id' is deprecated, use 'target_id'" in caplog.text


@pytest.mark.parametrize(
    ("yaml_file", "error"),
    [
        ("update_no_target.yaml", "Couldn't find any component that can be used for"),
        ("update_both_targets.yaml", "Cannot specify more than one of"),
        ("update_ambiguous_target.yaml", "Too many candidates found"),
    ],
)
def test_update_target_errors(
    component_config_path: Callable[[str], Path], yaml_file: str, error: str
) -> None:
    CORE.config_path = component_config_path(yaml_file)
    config = load_config({})
    assert any(error in str(err) for err in config.errors)
