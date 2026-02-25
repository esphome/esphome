from esphome import core
import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_ID, CONF_MAC_ADDRESS
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments, statement

CODEOWNERS = ["@jpeletier"]
DEPENDENCIES = ["esp32_ble_tracker"]

BLE_DEVICE_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA

bthome_ns = cg.esphome_ns.namespace("bthome")
DeviceListener = bthome_ns.class_(
    "DeviceListener", esp32_ble_tracker.ESPBTDeviceListener
)
DeviceBase = bthome_ns.class_("DeviceBase")
Device = bthome_ns.class_("Device", DeviceBase)
BTHomeObjectHandler = bthome_ns.class_("BTHomeObjectHandler")
BTHomeSensor = bthome_ns.class_(
    "BTHomeSensor", BTHomeObjectHandler, sensor.Sensor, cg.Component
)


class DeferredStatement(cg.Statement):
    def __init__(self, func):
        self.func = func

    def __str__(self):
        statements = []
        self.func(statements)
        code = []
        for exp in statements:
            text = str(statement(exp))
            text = text.rstrip()
            code.append(text)
        return "\n".join(code)


class DeferredExpression(cg.Expression):
    def __init__(self, func, *args, **kwargs):
        self.func = func
        self.args = args
        self.kwargs = kwargs

    def __str__(self):
        return str(self.func(*self.args, **self.kwargs))


_REMOTE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Device),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_BINDKEY): cv.bind_key,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeviceListener),
        cv.Optional("remote_devices"): [_REMOTE_DEVICE_SCHEMA],
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

    Called lazily at code-generation time (via DeferredExpression), after all
    handlers have been registered and the list is in its final sorted order.
    """
    for i, (_, hv) in enumerate(handlers):
        if hv is handler_var:
            return i
    raise ValueError("Handler not found in sorted list")


async def add_handler(
    handler_var: cg.MockObj, device_id: core.ID, object_id: int
) -> None:
    """Register handler_var with the Device identified by device_id.

    Appends (object_id, handler_var) to the per-device handler list and keeps
    the list sorted by object_id ascending — the same order that BTHome
    packets use — so that parse_data can scan both the packet objects and the
    handlers array in a single forward pass.

    The handler's array index is emitted as a DeferredExpression so it is
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
            DeferredExpression(_get_handler_index, handlers, handler_var),
            handler_var,
        )
    )


async def to_code(config):
    if "remote_devices" not in config:
        return

    listener = cg.new_Pvariable(
        core.ID(str(config[CONF_ID]), False, DeviceListener),
        TemplateArguments(len(config["remote_devices"])),
    )

    has_encryption = False
    for i, device_config in enumerate(config["remote_devices"]):
        device_id = device_config[CONF_ID]
        device_var = cg.Pvariable(
            core.ID(str(device_id), False, DeviceBase),
            DeferredExpression(
                lambda device_id: device_id.type.template(
                    TemplateArguments(_get_handler_count(device_id))
                ).new(),
                device_id,
            ),
        )

        cg.add(listener.set_device(i, device_var))
        cg.add(device_var.set_address(device_config[CONF_MAC_ADDRESS].as_hex))

        if CONF_BINDKEY in device_config:
            bindkey_str = device_config[CONF_BINDKEY]
            bindkey_bytes = [int(bindkey_str[i : i + 2], 16) for i in range(0, 32, 2)]
            cg.add(device_var.set_encryption_key(bindkey_bytes))
            has_encryption = True

    if has_encryption:
        cg.add_define("USE_BTHOME_DECRYPTION")

    await esp32_ble_tracker.register_ble_device(listener, config)
