from collections.abc import Callable
from typing import Any

from esphome import core
import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble_tracker, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY, CONF_MAC_ADDRESS
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments

CODEOWNERS = ["@jpeletier"]
DEPENDENCIES = ["esp32", "esp32_ble_tracker"]


BLE_DEVICE_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA

bthome_ns = cg.esphome_ns.namespace("bthome")
client_ns = bthome_ns.namespace("client")
DeviceListener = client_ns.class_("DeviceListener")
RemoteDeviceBase = client_ns.class_("RemoteDeviceBase")
RemoteDevice = client_ns.class_("RemoteDevice", RemoteDeviceBase)
BTHomeRemoteObject = bthome_ns.class_("BTHomeRemoteObject")
BTHomeSensor = client_ns.class_(
    "BTHomeSensor", BTHomeRemoteObject, sensor.Sensor, cg.Component
)
BTHomeBinarySensor = client_ns.class_(
    "BTHomeBinarySensor",
    BTHomeRemoteObject,
    binary_sensor.BinarySensor,
    cg.Component,
)
BTHomeTextSensor = client_ns.class_(
    "BTHomeTextSensor",
    BTHomeRemoteObject,
    text_sensor.TextSensor,
    cg.Component,
)
ESP32BLEListener = bthome_ns.class_(
    "ESP32BLEListener", esp32_ble_tracker.ESPBTDeviceListener
)


class BoundExpression(cg.Expression):
    """A code-generation expression whose value is computed lazily.

    Wraps a callable so that the expression string is not evaluated until
    ``__str__`` is called during code emission.  This is useful when the
    result of ``func`` depends on state that is not yet available at the
    point where the expression object is constructed (e.g. a symbol whose
    address is only known after all components have registered).
    """

    def __init__(
        self,
        func: Callable[..., cg.Expression],
        *args: Any,
        **kwargs: Any,
    ) -> None:
        self.func: Callable[..., cg.Expression] = func
        self.args: tuple[Any, ...] = args
        self.kwargs: dict[str, Any] = kwargs

    def __str__(self) -> str:
        return str(self.func(*self.args, **self.kwargs))


_REMOTE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RemoteDevice),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_KEY): cv.bind_key,
    }
)

CONF_REMOTE_DEVICES = "remote_devices"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_REMOTE_DEVICES): [_REMOTE_DEVICE_SCHEMA],
    }
).extend(BLE_DEVICE_SCHEMA)


BTHOME_KEY = "bthome_key"


def _get_handler_count(device_id: core.ID) -> int:
    """Return the number of handlers registered so far for the given device."""
    return len(CORE.data.get(BTHOME_KEY, {}).get(str(device_id), []))


def _get_handler_index(
    handlers: list[tuple[int, cg.MockObj]], handler_var: cg.MockObj
) -> int:
    """Return the position of handler_var in the sorted handlers list.

    Called lazily at code-generation time (via BoundExpression), after all
    handlers have been registered and the list is in its final sorted order.
    """
    for i, (_, hv) in enumerate(handlers):
        if hv is handler_var:
            return i
    raise ValueError("Handler not found in sorted list")


async def add_handler(
    handler_var: cg.MockObj, device_id: core.ID, object_id: int
) -> None:
    """Register handler_var with the RemoteDevice identified by device_id.

    Appends (object_id, handler_var) to the per-device handler list and keeps
    the list sorted by object_id ascending — the same order that BTHome
    packets use — so that parse_data can scan both the packet objects and the
    handlers array in a single forward pass.

    The handler's array index is emitted as a BoundExpression so it is
    resolved at code-generation time, after every handler across all sensor
    declarations has been added and the final sort order is known.
    """
    device_var = await cg.get_variable(device_id)
    devices = CORE.data.setdefault(BTHOME_KEY, {})
    handlers = devices.setdefault(str(device_id), [])
    handlers.append((object_id, handler_var))
    handlers.sort(key=lambda h: h[0])
    cg.add(
        device_var.set_handler(
            BoundExpression(_get_handler_index, handlers, handler_var),
            handler_var,
        )
    )


def _parse_key_bytes(key: str) -> list[int]:
    return [int(key[j : j + 2], 16) for j in range(0, 32, 2)]


async def to_code(config):
    listener_id = core.ID("bthome_listener", False, DeviceListener)
    listener = cg.new_Pvariable(
        listener_id,
        TemplateArguments(len(config[CONF_REMOTE_DEVICES])),
    )

    for i, device_config in enumerate(config[CONF_REMOTE_DEVICES]):
        device_id = device_config[CONF_ID]
        device_var = cg.Pvariable(
            core.ID(str(device_id), False, RemoteDeviceBase),
            BoundExpression(
                lambda device_id: device_id.type.template(
                    TemplateArguments(_get_handler_count(device_id))
                ).new(),
                device_id,
            ),
        )

        cg.add(listener.set_device(i, device_var))
        cg.add(device_var.set_address(device_config[CONF_MAC_ADDRESS].as_hex))

        if key := device_config.get(CONF_KEY):
            cg.add(device_var.set_encryption_key(_parse_key_bytes(key)))
            cg.add_define("USE_BTHOME_DECRYPTION")

    ble_listener_id = core.ID("bthome_ble_listener", False, ESP32BLEListener)
    ble_listener = cg.new_Pvariable(ble_listener_id)
    cg.add(ble_listener.setup(listener))
    await esp32_ble_tracker.register_ble_device(ble_listener, config)
