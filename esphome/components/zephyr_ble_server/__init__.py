from dataclasses import dataclass

from esphome import automation
import esphome.codegen as cg
from esphome.components.nrf52.framework import (
    bluetooth_west_projects,
    include_west_project,
)
from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_variant
from esphome.components.zephyr.variants import VARIANTS
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

# BLE LE Data Length Extension maximum LL PDU payload (BLE spec)
_DLE_MAX_PDU = 251

DOMAIN = "zephyr_ble_server"


@dataclass
class _BLEServerData:
    requested_l2cap_mtu: int = 0
    mtu_emitted: bool = False


def _get_data() -> _BLEServerData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _BLEServerData()
    return CORE.data[DOMAIN]


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_ble_mtu() -> None:
    data = _get_data()
    data.mtu_emitted = True
    mtu = data.requested_l2cap_mtu
    if mtu > 0:
        zephyr_add_prj_conf("BT_L2CAP_TX_MTU", mtu)
        zephyr_add_prj_conf("BT_BUF_ACL_TX_SIZE", min(mtu + 4, _DLE_MAX_PDU))
        zephyr_add_prj_conf("BT_BUF_ACL_RX_SIZE", mtu + 4)


def request_ble_l2cap_mtu(l2cap_mtu: int) -> None:
    """Request a minimum BLE L2CAP MTU. The maximum of all callers wins."""
    data = _get_data()
    if data.mtu_emitted:
        raise RuntimeError("BLE L2CAP MTU requested after it was written to prj.conf")
    if data.requested_l2cap_mtu == 0:
        CORE.add_job(_emit_ble_mtu)
    data.requested_l2cap_mtu = max(data.requested_l2cap_mtu, l2cap_mtu)


zephyr_ble_server_ns = cg.esphome_ns.namespace("zephyr_ble_server")
BLEServer = zephyr_ble_server_ns.class_("BLEServer", cg.Component)

CONF_ON_NUMERIC_COMPARISON_REQUEST = "on_numeric_comparison_request"
CONF_ACCEPT = "accept"


def _validate_variant(config):
    # The standalone `platform: nrf52` target has no `variant`/VARIANTS entry --
    # BLE is inherent to all its boards, so there's nothing to check.
    if CORE.is_nrf52:
        return config
    variant_name = zephyr_variant()
    variant = VARIANTS.get(variant_name)
    if variant is None or "ble" not in variant.transports:
        raise cv.Invalid(f"ble is not supported on Zephyr variant '{variant_name}'")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLEServer),
            cv.Optional(
                CONF_ON_NUMERIC_COMPARISON_REQUEST
            ): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
    _validate_variant,
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_NUMERIC_COMPARISON_REQUEST,
        "add_passkey_callback",
        [(cg.uint32, "passkey")],
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("BT", True)
    if CORE.is_nrf52:
        for project in bluetooth_west_projects():
            include_west_project(project)
    zephyr_add_prj_conf("BT_PERIPHERAL", True)
    zephyr_add_prj_conf("BT_RX_STACK_SIZE", 1536)
    zephyr_add_prj_conf("BT_DEVICE_NAME", CORE.name)
    await cg.register_component(var, config)
    if config.get(CONF_ON_NUMERIC_COMPARISON_REQUEST):
        zephyr_add_prj_conf("BT_SMP", True)
        zephyr_add_prj_conf("BT_SETTINGS", True)
        zephyr_add_prj_conf("BT_SMP_SC_ONLY", True)
        zephyr_add_prj_conf("BT_KEYS_OVERWRITE_OLDEST", True)
        request_ble_l2cap_mtu(65)  # BT_SMP Kconfig default and range minimum
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(BLEServer),
        cv.Required(CONF_ACCEPT): cv.templatable(cv.boolean),
    }
)


automation.register_apply_action(
    "ble_server.numeric_comparison_reply",
    BLE_NUMERIC_COMPARISON_REPLY_ACTION_SCHEMA,
    automation.ApplyField(CONF_ACCEPT, "numeric_comparison_reply", cg.bool_),
)
