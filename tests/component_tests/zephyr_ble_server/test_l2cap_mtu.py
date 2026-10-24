"""Tests for the BLE L2CAP MTU that zephyr_ble_server writes to prj.conf."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_PRJ_CONF
from esphome.components.zephyr_ble_server import _emit_ble_mtu, request_ble_l2cap_mtu

MTU_KEYS = (
    "CONFIG_BT_L2CAP_TX_MTU",
    "CONFIG_BT_BUF_ACL_TX_SIZE",
    "CONFIG_BT_BUF_ACL_RX_SIZE",
)


def _mtu_values() -> tuple[int | None, ...]:
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    return tuple(prj_conf.get(key, (None,))[0] for key in MTU_KEYS)


@pytest.mark.parametrize(
    ("fixture", "expected"),
    [
        ("no_request.yaml", (None, None, None)),
        ("nus.yaml", (247, 251, 251)),
        # The largest request wins; only the TX buffer is capped at 251
        ("nus_mcumgr.yaml", (498, 251, 502)),
    ],
)
def test_l2cap_mtu_prj_conf(
    fixture: str,
    expected: tuple[int | None, ...],
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    assert _mtu_values() == expected


@pytest.mark.asyncio
async def test_request_after_emit_raises(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("no_request.yaml"))
    request_ble_l2cap_mtu(247)
    await _emit_ble_mtu()
    with pytest.raises(RuntimeError, match="after it was written"):
        request_ble_l2cap_mtu(498)
