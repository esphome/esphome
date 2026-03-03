from esphome import core
import esphome.codegen as cg
from esphome.components import (
    binary_sensor,
    esp32_ble,
    esp32_ble_tracker,
    sensor,
    text_sensor,
)
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_ID, CONF_MAC_ADDRESS, CONF_TYPE
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments, statement

from .bthome import (
    BTHOME_OBJECT_TYPES,
    BTHOME_SERVER_MAX_ENCRYPTED_PAYLOAD,
    BTHOME_SERVER_MAX_PAYLOAD,
    OBJECT_TYPES_BY_ID,
    BTHomeObjectTypeKind,
    bthome_object_type_validator,
)

CODEOWNERS = ["@jpeletier"]
DEPENDENCIES = ["esp32", "esp32_ble_tracker"]


def AUTO_LOAD():
    if CORE.cpp_testing:
        return ["esp32_ble", "text_sensor", "binary_sensor", "sensor"]
    return ["esp32_ble"]


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
BTHomeESP32BLEAdvertiser = bthome_ns.class_("ESP32BLEAdvertiser")
ESP32BLEListener = bthome_ns.class_(
    "ESP32BLEListener", esp32_ble_tracker.ESPBTDeviceListener
)

# Server-side classes
server_ns = bthome_ns.namespace("server")
BTHomeServerBase = server_ns.class_(
    "BTHomeServerBase", cg.Component, esp32_ble.GAPEventHandler
)
BTHomeServer = server_ns.class_("BTHomeServer", BTHomeServerBase)
BTHomeLocalSensor = server_ns.class_("BTHomeLocalSensor")
BTHomeLocalBinarySensor = server_ns.class_("BTHomeLocalBinarySensor")
BTHomeLocalTextSensor = server_ns.class_("BTHomeLocalTextSensor")


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
        cv.GenerateID(): cv.declare_id(RemoteDevice),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_BINDKEY): cv.bind_key,
    }
)

# Server-side config keys
CONF_REMOTE_DEVICES = "remote_devices"
CONF_SENSORS = "sensors"
CONF_BINARY_SENSORS = "binary_sensors"
CONF_TEXT_SENSORS = "text_sensors"
CONF_ADVERTISE_IMMEDIATELY = "advertise_immediately"
CONF_MAX_LENGTH = "max_length"


def _get_value_length(object_id: int) -> int:
    """Return the encoded byte length for a BTHome object type."""
    if object_id < 0 or object_id >= len(OBJECT_TYPES_BY_ID):
        return 0

    return OBJECT_TYPES_BY_ID[object_id].size


_SERVER_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TYPE): bthome_object_type_validator(
            BTHomeObjectTypeKind.SENSOR
        ),
        cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ADVERTISE_IMMEDIATELY, default=False): cv.boolean,
    }
)

_SERVER_BINARY_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TYPE): bthome_object_type_validator(
            BTHomeObjectTypeKind.BINARY_SENSOR
        ),
        cv.Required(CONF_ID): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_ADVERTISE_IMMEDIATELY, default=False): cv.boolean,
    }
)

_SERVER_TEXT_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TYPE): bthome_object_type_validator(
            BTHomeObjectTypeKind.TEXT_SENSOR
        ),
        cv.Required(CONF_ID): cv.use_id(text_sensor.TextSensor),
        cv.Optional(CONF_MAX_LENGTH, default=5): cv.int_range(
            min=1, max=BTHOME_SERVER_MAX_PAYLOAD - 2
        ),
        cv.Optional(CONF_ADVERTISE_IMMEDIATELY, default=False): cv.boolean,
    }
)


def _validate_server_config(config):
    """Validate server-side sensor configuration constraints."""
    has_server = (
        CONF_SENSORS in config
        or CONF_BINARY_SENSORS in config
        or CONF_TEXT_SENSORS in config
    )
    if not has_server:
        return config

    max_payload = (
        BTHOME_SERVER_MAX_ENCRYPTED_PAYLOAD
        if CONF_BINDKEY in config
        else BTHOME_SERVER_MAX_PAYLOAD
    )

    # Collect all entries with their object IDs
    entries_by_type = {}
    for entry in config.get(CONF_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        entries_by_type.setdefault(ot.object_id, []).append(entry)
    for entry in config.get(CONF_BINARY_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        entries_by_type.setdefault(ot.object_id, []).append(entry)
    for entry in config.get(CONF_TEXT_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        entries_by_type.setdefault(ot.object_id, []).append(entry)

    for object_id, entries in entries_by_type.items():
        ot = OBJECT_TYPES_BY_ID[object_id]
        # Validate same-type sensors fit in one frame
        if ot.kind == BTHomeObjectTypeKind.TEXT_SENSOR:
            # Variable-length: type(1) + length_byte(1) + content(max_length)
            total_size = sum(2 + e[CONF_MAX_LENGTH] for e in entries)
        else:
            value_length = _get_value_length(object_id)
            total_size = len(entries) * (1 + value_length)
        if total_size > max_payload:
            raise cv.Invalid(
                f"Sensors of type 0x{object_id:02X} require {total_size} bytes "
                f"but max payload is {max_payload} bytes"
            )

        # Validate advertise_immediately consistency
        imm_values = {e.get(CONF_ADVERTISE_IMMEDIATELY, False) for e in entries}
        if len(imm_values) > 1:
            raise cv.Invalid(
                f"All sensors of the same object_type (0x{object_id:02X}) must have "
                f"the same advertise_immediately setting"
            )

    return config


def _validate_has_content(config):
    """Ensure at least remote_devices or server sensors are configured."""
    has_client = CONF_REMOTE_DEVICES in config
    has_server = (
        CONF_SENSORS in config
        or CONF_BINARY_SENSORS in config
        or CONF_TEXT_SENSORS in config
    )
    if not has_client and not has_server:
        raise cv.Invalid(
            "At least one of 'remote_devices', 'sensors', 'binary_sensors', "
            "or 'text_sensors' must be configured"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
            cv.GenerateID(): cv.declare_id(BTHomeServerBase),
            cv.Optional(CONF_REMOTE_DEVICES): [_REMOTE_DEVICE_SCHEMA],
            cv.Optional(CONF_BINDKEY): cv.bind_key,
            cv.Optional(CONF_SENSORS): [_SERVER_SENSOR_SCHEMA],
            cv.Optional(CONF_BINARY_SENSORS): [_SERVER_BINARY_SENSOR_SCHEMA],
            cv.Optional(CONF_TEXT_SENSORS): [_SERVER_TEXT_SENSOR_SCHEMA],
        }
    ).extend(BLE_DEVICE_SCHEMA),
    _validate_has_content,
    _validate_server_config,
)


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
    """Register handler_var with the RemoteDevice identified by device_id.

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


async def _client_to_code(config):
    """Generate code for client-side (remote devices)."""
    # Need a DeviceListener ID — generate one programmatically
    listener_id = core.ID("bthome_listener", False, DeviceListener)
    listener = cg.new_Pvariable(
        listener_id,
        TemplateArguments(len(config[CONF_REMOTE_DEVICES])),
    )

    has_encryption = False
    for i, device_config in enumerate(config[CONF_REMOTE_DEVICES]):
        device_id = device_config[CONF_ID]
        device_var = cg.Pvariable(
            core.ID(str(device_id), False, RemoteDeviceBase),
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
            bindkey_bytes = [int(bindkey_str[j : j + 2], 16) for j in range(0, 32, 2)]
            cg.add(device_var.set_encryption_key(bindkey_bytes))
            has_encryption = True

    if has_encryption:
        cg.add_define("USE_BTHOME_DECRYPTION")

    ble_listener_id = core.ID("bthome_ble_listener", False, ESP32BLEListener)
    ble_listener = cg.new_Pvariable(ble_listener_id)
    cg.add(ble_listener.setup(listener))
    await esp32_ble_tracker.register_ble_device(ble_listener, config)


async def _server_to_code(config):
    """Generate code for server-side (advertising local sensors)."""
    from .bthome import bthome_object_types

    # Enable server-side compilation
    cg.add_define("USE_BTHOME_SERVER")

    # Merge and sort all sensor entries by object_id (stable sort)
    all_entries = []
    for entry in config.get(CONF_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        all_entries.append((ot.object_id, "sensor", entry))
    for entry in config.get(CONF_BINARY_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        all_entries.append((ot.object_id, "binary_sensor", entry))
    for entry in config.get(CONF_TEXT_SENSORS, []):
        ot = BTHOME_OBJECT_TYPES[entry[CONF_TYPE]]
        all_entries.append((ot.object_id, "text_sensor", entry))

    # Stable sort by object_id ascending
    all_entries.sort(key=lambda e: e[0])

    n = len(all_entries)
    # Create BTHomeServer<N>
    esp32_ble_adapter = cg.new_Pvariable(
        core.ID("bthome_ble_adapter", False, BTHomeESP32BLEAdvertiser)
    )
    server_id = config[CONF_ID]
    server_var = cg.new_Pvariable(
        core.ID(str(server_id), False, BTHomeServer),
        TemplateArguments(n),
        esp32_ble_adapter,
    )

    await cg.register_component(server_var, {})

    # Register GAP event handler
    ble_var = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(ble_var, esp32_ble_adapter)

    # Encryption
    if CONF_BINDKEY in config:
        bindkey_str = config[CONF_BINDKEY]
        bindkey_bytes = [int(bindkey_str[j : j + 2], 16) for j in range(0, 32, 2)]
        cg.add(server_var.set_encryption_key(bindkey_bytes))
        cg.add_define("USE_BTHOME_ENCRYPTION")

    cg.add_define("USE_ESP32_BLE_ADVERTISING")

    # Create local sensor wrappers
    for i, (object_id, kind, entry) in enumerate(all_entries):
        ot_key = entry[CONF_TYPE]

        if kind == "sensor":
            local_id = core.ID(f"bthome_local_{i}", False, BTHomeLocalSensor)
            local_var = cg.new_Pvariable(local_id)
            source = await cg.get_variable(entry[CONF_ID])
            cg.add(local_var.set_source(source))
        elif kind == "text_sensor":
            local_id = core.ID(f"bthome_local_{i}", False, BTHomeLocalTextSensor)
            local_var = cg.new_Pvariable(local_id)
            source = await cg.get_variable(entry[CONF_ID])
            cg.add(local_var.set_source(source))
            cg.add(local_var.set_max_length(entry[CONF_MAX_LENGTH]))
        else:
            local_id = core.ID(f"bthome_local_{i}", False, BTHomeLocalBinarySensor)
            local_var = cg.new_Pvariable(local_id)
            source = await cg.get_variable(entry[CONF_ID])
            cg.add(local_var.set_source(source))

        cg.add(local_var.set_object_type(bthome_object_types.__getattr__(ot_key)))

        if entry.get(CONF_ADVERTISE_IMMEDIATELY, False):
            cg.add(local_var.set_advertise_immediately(True))

        cg.add(server_var.set_local_sensor(i, local_var))


async def to_code(config):
    if CONF_REMOTE_DEVICES in config:
        await _client_to_code(config)

    if (
        CONF_SENSORS in config
        or CONF_BINARY_SENSORS in config
        or CONF_TEXT_SENSORS in config
    ):
        await _server_to_code(config)


# This function is executed instead of to_code() during c++ testing
async def to_code_testing(config):
    # During testing, enable encryption code unconditionally
    cg.add_define("USE_BTHOME_ENCRYPTION")
    cg.add_define("USE_BTHOME_DECRYPTION")

    # During testing, enable all sensor types unconditionally.
    # The entity count defines are normally generated from CORE.platform_counts by
    # _add_platform_defines(). Sensor and binary_sensor counts are set via their
    # platform components (sensor.bthome / binary_sensor.bthome). Text sensor has
    # no bthome platform component, so we set its count explicitly here.
    cg.add_define("USE_SENSOR")
    cg.add_define("USE_BINARY_SENSOR")
    cg.add_define("USE_TEXT_SENSOR")
    cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 1)

    # Pull mbedtls for testing in host environment
    cg.add_library("baracodadailyhealthtech/mbedtls", "3.6.1-1", None)
