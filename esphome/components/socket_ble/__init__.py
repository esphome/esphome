from collections.abc import Callable, MutableMapping
from dataclasses import dataclass, field
from enum import StrEnum

import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_OTA, CONF_PLATFORM, PLATFORM_NRF52
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["zephyr_ble_server"]
DOMAIN = "socket_ble"

CONF_MTU = "mtu"


class SocketType(StrEnum):
    L2CAP = "l2cap"
    L2CAP_LISTEN = "l2cap_listen"


@dataclass
class SocketBleData:
    consumers: dict[SocketType, dict[str, int]] = field(
        default_factory=lambda: {socket_type: {} for socket_type in SocketType}
    )


def _get_data() -> SocketBleData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SocketBleData()
    return CORE.data[DOMAIN]


def consume_sockets(
    value: int, consumer: str, socket_type: SocketType = SocketType.L2CAP
) -> Callable[[MutableMapping], MutableMapping]:
    """Register socket usage for a component.

    Args:
        value: Number of sockets needed by the component
        consumer: Name of the component consuming the sockets
        socket_type: Type of socket (SocketType.L2CAP or SocketType.L2CAP_LISTEN)

    Returns:
        A validator function that records the socket usage
    """

    def _consume_sockets(config: MutableMapping) -> MutableMapping:
        consumers = _get_data().consumers[socket_type]
        consumers[consumer] = consumers.get(consumer, 0) + value
        return config

    return _consume_sockets


def get_socket_count(socket_type: SocketType) -> int:
    """Return how many sockets of a type the components asked for."""
    return sum(_get_data().consumers[socket_type].values())


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MTU, default=160): cv.int_range(min=100, max=65535),
        }
    ),
    cv.only_on([PLATFORM_NRF52]),
)


# MTU that OTA over BLE configures for itself (NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP)
BLE_OTA_MTU = 498


def _has_ble_ota() -> bool:
    return any(
        conf.get(CONF_PLATFORM) == "zephyr_mcumgr"
        and conf.get("transport", {}).get("ble")
        for conf in CORE.config.get(CONF_OTA, [])
    )


def _other_ble_links() -> int:
    """Links to keep free for BLE users that are not sockets."""
    return ("ble_nus" in CORE.config) + _has_ble_ota()


async def to_code(config: ConfigType) -> None:
    socket_count = get_socket_count(SocketType.L2CAP)
    mtu = config[CONF_MTU]
    cg.add_define("SOCKET_BLE_COUNT", socket_count)
    cg.add_define("SOCKET_BLE_LISTEN_COUNT", get_socket_count(SocketType.L2CAP_LISTEN))
    cg.add_define("SOCKET_BLE_MTU", mtu)

    zephyr_add_prj_conf("BT_SMP", True)
    zephyr_add_prj_conf("BT_L2CAP_DYNAMIC_CHANNEL", True)
    zephyr_add_prj_conf("BT_MAX_CONN", socket_count + _other_ble_links())
    # OTA over BLE sets larger buffers itself; smaller values here would break its build
    if not _has_ble_ota() or mtu > BLE_OTA_MTU:
        zephyr_add_prj_conf("BT_BUF_ACL_RX_SIZE", mtu + 4)
        zephyr_add_prj_conf("BT_L2CAP_TX_MTU", mtu)
