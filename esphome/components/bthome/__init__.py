from dataclasses import dataclass, field

from esphome import core
import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY, CONF_MAC_ADDRESS
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.cpp_generator import TemplateArguments
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]
CODEOWNERS = ["@jpeletier"]
DOMAIN = "bthome"

bthome_ns = cg.esphome_ns.namespace("bthome")
client_ns = bthome_ns.namespace("client")
DeviceListener = client_ns.class_(
    "DeviceListener", ble_device_base.ESPBTDeviceListener, cg.Component
)
RemoteDeviceBase = client_ns.class_("RemoteDeviceBase")
RemoteDevice = client_ns.class_("RemoteDevice", RemoteDeviceBase)
BTHomeRemoteObject = client_ns.class_("BTHomeRemoteObject")

_REMOTE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RemoteDevice),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_KEY): cv.bind_key,
    }
)

CONF_REMOTE_DEVICES = "remote_devices"


def _validate_unique_macs(devices: list[ConfigType]) -> list[ConfigType]:
    """Reject remote devices that share a MAC address: only the first would get data."""
    seen: set[str] = set()
    for device in devices:
        mac = str(device[CONF_MAC_ADDRESS])
        if mac in seen:
            raise cv.Invalid(f"Duplicate remote device MAC address: {mac}")
        seen.add(mac)
    return devices


CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id(DOMAIN),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DeviceListener),
            cv.Required(CONF_REMOTE_DEVICES): cv.All(
                [_REMOTE_DEVICE_SCHEMA], _validate_unique_macs
            ),
        }
    )
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)


@dataclass
class BTHomeData:
    # Handlers registered by the sensor platforms, keyed by remote device ID.
    handlers: dict[str, list[tuple[int, cg.MockObj]]] = field(default_factory=dict)


def _get_data() -> BTHomeData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = BTHomeData()
    return CORE.data[DOMAIN]


def add_handler(handler_var: cg.MockObj, device_id: core.ID, object_id: int) -> None:
    """Register handler_var with the remote device identified by device_id.

    The device itself is created later, once every platform has registered its
    handlers, so that its handler array can be sized at compile time.
    """
    _get_data().handlers.setdefault(str(device_id), []).append((object_id, handler_var))


def _parse_key_bytes(key: str) -> list[int]:
    return [int(key[j : j + 2], 16) for j in range(0, 32, 2)]


@coroutine_with_priority(CoroPriority.FINAL)
async def _create_remote_devices(listener: cg.MockObj, config: ConfigType) -> None:
    data = _get_data()
    for i, device_config in enumerate(config[CONF_REMOTE_DEVICES]):
        device_id = device_config[CONF_ID]
        # parse_data scans packet objects and handlers in a single forward pass, so the
        # handlers must be in the same ascending object ID order as BTHome packets.
        # The sort is stable, so handlers of the same type keep their configuration order.
        handlers = sorted(data.handlers.get(str(device_id), []), key=lambda h: h[0])
        device_var = cg.new_Pvariable(device_id, TemplateArguments(len(handlers)))
        cg.add(device_var.set_address(device_config[CONF_MAC_ADDRESS].as_hex))
        if key := device_config.get(CONF_KEY):
            cg.add(device_var.set_encryption_key(_parse_key_bytes(key)))
            cg.add_define("USE_BTHOME_DECRYPTION")
        for index, (_, handler_var) in enumerate(handlers):
            cg.add(device_var.set_handler(index, handler_var))
        cg.add(listener.set_device(i, device_var))


async def to_code(config: ConfigType) -> None:
    listener = cg.new_Pvariable(
        config[CONF_ID],
        TemplateArguments(len(config[CONF_REMOTE_DEVICES])),
    )
    await cg.register_component(listener, config)
    await ble_device_base.register_ble_device(listener, config)
    CORE.add_job(_create_remote_devices, listener, config)
