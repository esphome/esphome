from collections.abc import Callable, MutableMapping
from dataclasses import dataclass
from enum import StrEnum
import logging

import esphome.codegen as cg
from esphome.components import zephyr
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    PLATFORM_ESP32,
    PLATFORM_NRF52,
    PLATFORM_RP2040,
    PlatformFramework,
)
from esphome.core import CORE

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]

CONF_MTU = "mtu"

# Socket tracking infrastructure
# Components register their socket needs and platforms read this to configure appropriately
KEY_SOCKET_CONSUMERS = "socket_consumers"
KEY_SOCKET_CONSUMERS_LISTEN = "socket_consumers_listen"


def AUTO_LOAD():
    if CORE.is_nrf52:
        return ["zephyr_ble_server"]
    if CORE.is_rp2:
        return ["rp2040_ble"]
    return []


# Incompatible with esp32_ble as this component uses NimBLE and esp32_ble uses Bluedroid.
# Both Bluetooth stacks cannot be enabled at the same time.
CONFLICTS_WITH = ["esp32_ble"]


class SocketType(StrEnum):
    L2CAP = "l2cap"
    L2CAP_LISTEN = "l2cap_listen"


_SOCKET_TYPE_KEYS = {
    SocketType.L2CAP: KEY_SOCKET_CONSUMERS,
    SocketType.L2CAP_LISTEN: KEY_SOCKET_CONSUMERS_LISTEN,
}


def consume_sockets(
    value: int, consumer: str, socket_type: SocketType = SocketType.L2CAP
) -> Callable[[MutableMapping], MutableMapping]:
    """Register socket usage for a component.

    Args:
        value: Number of sockets needed by the component
        consumer: Name of the component consuming the sockets
        socket_type: Type of socket (SocketType.TCP, SocketType.UDP, or SocketType.TCP_LISTEN)

    Returns:
        A validator function that records the socket usage
    """
    typed_key = _SOCKET_TYPE_KEYS[socket_type]

    def _consume_sockets(config: MutableMapping) -> MutableMapping:
        consumers: dict[str, int] = CORE.data.setdefault(typed_key, {})
        consumers[consumer] = consumers.get(consumer, 0) + value
        return config

    return _consume_sockets


def _format_consumers(consumers: dict[str, int]) -> str:
    """Format consumer dict as 'name=count, ...' or 'none'."""
    if not consumers:
        return "none"
    return ", ".join(f"{name}={count}" for name, count in sorted(consumers.items()))


@dataclass(frozen=True)
class SocketCounts:
    """Socket counts and component details for platform configuration."""

    l2cap: int
    l2cap_listen: int
    l2cap_details: str
    l2cap_listen_details: str


def get_socket_counts() -> SocketCounts:
    """Return socket counts and component details for platform configuration.

    Platforms call this during code generation to configure lwIP socket limits.
    All components will have registered their needs by then.

    Platforms should apply their own minimums on top of these values.
    """
    l2cap_consumers = CORE.data.get(KEY_SOCKET_CONSUMERS, {})
    l2cap_listen_consumers = CORE.data.get(KEY_SOCKET_CONSUMERS_LISTEN, {})
    l2cap = sum(l2cap_consumers.values())
    l2cap_listen = sum(l2cap_listen_consumers.values())

    l2cap_details = _format_consumers(l2cap_consumers)
    l2cap_listen_details = _format_consumers(l2cap_listen_consumers)
    _LOGGER.debug(
        "Socket counts: L2CAP=%d (%s), L2CAP_LISTEN=%d (%s)",
        l2cap,
        l2cap_details,
        l2cap_listen,
        l2cap_listen_details,
    )
    return SocketCounts(
        l2cap=l2cap,
        l2cap_listen=l2cap_listen,
        l2cap_details=l2cap_details,
        l2cap_listen_details=l2cap_listen_details,
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MTU, default=160): cv.int_range(
                min=100, min_included=True
            ),
        }
    ),
    cv.only_on(
        [
            PLATFORM_ESP32,
            PLATFORM_NRF52,
            PLATFORM_RP2040,
        ]
    ),
)


async def to_code(config):
    socket_counts = get_socket_counts()
    cg.add_define("SOCKET_BLE_COUNT", socket_counts.l2cap)
    cg.add_define("SOCKET_BLE_LISTEN_COUNT", socket_counts.l2cap_listen)
    cg.add_define("SOCKET_BLE_MTU", config[CONF_MTU])

    if CORE.is_esp32:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", False)
        add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
        add_idf_sdkconfig_option(
            "CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM", socket_counts.l2cap
        )
        add_idf_sdkconfig_option("CONFIG_BT_CONTROLLER_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ROLE_PERIPHERAL", True)
        cg.add_define("BLE_DEVICE_NAME", CORE.name)
    elif CORE.using_zephyr:
        zephyr.zephyr_add_prj_conf("BT_SMP", True)
        zephyr.zephyr_add_prj_conf("BT_L2CAP_DYNAMIC_CHANNEL", True)
        zephyr.zephyr_add_prj_conf("BT_MAX_CONN", socket_counts.l2cap)
        zephyr.zephyr_add_prj_conf("BT_BUF_ACL_RX_SIZE", config[CONF_MTU] + 4)
        zephyr.zephyr_add_prj_conf("BT_L2CAP_TX_MTU", config[CONF_MTU])


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "esp32_sockets_l2cap_impl.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "zephyr_sockets_l2cap_impl.cpp": {PlatformFramework.NRF52_ZEPHYR},
        "rp2040_sockets_l2cap_impl.cpp": {PlatformFramework.RP2040_ARDUINO},
    }
)
