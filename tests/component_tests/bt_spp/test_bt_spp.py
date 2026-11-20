"""Tests for the bt_spp code generation and validation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS
from esphome.config import read_config
from esphome.core import CORE
from esphome.yaml_util import Sensitive

_BASE = """
esphome:
  name: test

esp32:
  variant: {variant}
  framework:
    type: {framework}
"""

_BT_SPP = """
bt_spp:
  id: obd
  mac_address: "8C:DE:52:CF:1C:4F"
"""


def _write(
    tmp_path: Path, body: str, variant: str = "esp32", framework: str = "esp-idf"
) -> Path:
    path = tmp_path / "test.yaml"
    path.write_text(_BASE.format(variant=variant, framework=framework) + body)
    return path


def _sdkconfig() -> dict[str, object]:
    return CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]


def test_defaults(generate_main: Callable[[str | Path], str], tmp_path: Path) -> None:
    main_cpp = generate_main(_write(tmp_path, _BT_SPP))

    # MAC address, then one static block of 512 + 1 RX and 256 TX bytes
    assert (
        "bt_spp::BtSppComponent(0x8CDE52CF1C4FULL, bt_spp__obd_buffer__pstorage"
        in main_cpp
    )
    assert "bt_spp__obd_buffer__pstorage[769];" in main_cpp
    assert ", 512, 256);" in main_cpp
    assert "obd->set_reconnect_interval(10000);" in main_cpp
    for setter in ("set_channel", "set_pin_code", "set_name", "set_enable_on_boot"):
        assert setter not in main_cpp

    sdkconfig = _sdkconfig()
    assert sdkconfig["CONFIG_BT_CLASSIC_ENABLED"] is True
    assert sdkconfig["CONFIG_BT_SPP_ENABLED"] is True
    assert sdkconfig["CONFIG_BT_BLE_ENABLED"] is False
    assert sdkconfig["CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY"] is True


def test_options(generate_main: Callable[[str | Path], str], tmp_path: Path) -> None:
    main_cpp = generate_main(
        _write(
            tmp_path,
            _BT_SPP
            + """  channel: 3
  pin_code: "0000"
  name: obd-reader
  enable_on_boot: false
  reconnect_interval: 30s
  rx_buffer_size: 2048
  tx_buffer_size: 64
""",
        )
    )
    assert "obd->set_channel(3);" in main_cpp
    assert 'obd->set_pin_code("0000");' in main_cpp
    assert isinstance(CORE.config["bt_spp"]["pin_code"], Sensitive)
    assert 'obd->set_name("obd-reader");' in main_cpp
    assert "obd->set_enable_on_boot(false);" in main_cpp
    assert "obd->set_reconnect_interval(30000);" in main_cpp
    assert "bt_spp__obd_buffer__pstorage[2113];" in main_cpp
    assert ", 2048, 64);" in main_cpp


def test_wifi_requests_coexistence(
    generate_main: Callable[[str | Path], str], tmp_path: Path
) -> None:
    generate_main(
        _write(tmp_path, _BT_SPP + "\nwifi:\n  ssid: test\n  password: password1\n")
    )
    assert _sdkconfig()["CONFIG_ESP_COEX_SW_COEXIST_ENABLE"] is True


@pytest.mark.parametrize(
    ("variant", "framework", "body"),
    [
        pytest.param("esp32s3", "esp-idf", _BT_SPP, id="no_classic_on_s3"),
        pytest.param("esp32c3", "esp-idf", _BT_SPP, id="no_classic_on_c3"),
        pytest.param("esp32", "esp-idf", _BT_SPP + "esp32_ble_tracker:\n", id="ble"),
        pytest.param("esp32", "esp-idf", _BT_SPP + "  channel: 31\n", id="channel"),
        pytest.param(
            "esp32", "esp-idf", _BT_SPP + '  pin_code: "12345678901234567"\n', id="pin"
        ),
        # 9 characters, but 18 bytes: the PIN reply holds 16 bytes
        pytest.param(
            "esp32", "esp-idf", _BT_SPP + '  pin_code: "ééééééééé"\n', id="pin_bytes"
        ),
        pytest.param(
            "esp32", "esp-idf", _BT_SPP + "  reconnect_interval: 500ms\n", id="interval"
        ),
    ],
)
def test_invalid(tmp_path: Path, variant: str, framework: str, body: str) -> None:
    CORE.config_path = _write(tmp_path, body, variant, framework)
    assert read_config({}) is None
