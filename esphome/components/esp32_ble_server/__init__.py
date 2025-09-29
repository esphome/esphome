import encodings
from typing import TypeAlias

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_ble
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import BTLoggers, bt_uuid
import esphome.config_validation as cv
from esphome.config_validation import UNDEFINED
from esphome.const import (
    CONF_DATA,
    CONF_ESPHOME,
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_MODEL,
    CONF_NOTIFY,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_PROJECT,
    CONF_SERVICES,
    CONF_TYPE,
    CONF_UUID,
    CONF_VALUE,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE
from esphome.schema_extractors import SCHEMA_EXTRACT

AUTO_LOAD = ["esp32_ble", "bytebuffer", "event_emitter"]
CODEOWNERS = ["@jesserockz", "@clydebarrow", "@Rapsssito"]
DEPENDENCIES = ["esp32"]
DOMAIN = "esp32_ble_server"

# Type aliases
_ListenerAllocation: TypeAlias = tuple[str, int | str, int]  # (component, uuid, count)
_ServerListenerAllocation: TypeAlias = tuple[str, int]  # (component, count)

# Event listener allocation tracking - used by components to reserve slots
_LISTENER_ALLOCATIONS: dict[
    str, list[_ListenerAllocation | _ServerListenerAllocation]
] = {
    "characteristic_write": [],
    "characteristic_read": [],
    "descriptor_write": [],
    "server_connect": [],
    "server_disconnect": [],
}


def allocate_characteristic_event_listener(
    uuid: int | str, event_type: str, component: str, count: int = 1
) -> None:
    """
    Allocate event listener slots for a characteristic.

    Args:
        uuid: The characteristic UUID (int or string)
        event_type: "WRITE" or "READ"
        component: Name of the component requesting allocation
        count: Number of listeners needed (default 1)
    """
    if event_type not in ("WRITE", "READ"):
        raise ValueError(f"Unknown event_type: {event_type}")

    key = f"characteristic_{event_type.lower()}"
    _LISTENER_ALLOCATIONS[key].append((component, uuid, count))


def allocate_descriptor_event_listener(
    uuid: int | str, event_type: str, component: str, count: int = 1
) -> None:
    """Allocate event listener slots for a descriptor."""
    if event_type != "WRITE":
        raise ValueError(f"Unknown event_type: {event_type}")

    _LISTENER_ALLOCATIONS["descriptor_write"].append((component, uuid, count))


def allocate_server_event_listener(
    event_type: str, component: str, count: int = 1
) -> None:
    """Allocate event listener slots for server events."""
    if event_type not in ("CONNECT", "DISCONNECT"):
        raise ValueError(f"Unknown event_type: {event_type}")

    key = f"server_{event_type.lower()}"
    _LISTENER_ALLOCATIONS[key].append((component, count))


def _sum_allocations_for_uuid(allocation_key: str, uuid: int | str) -> int:
    """Helper to sum allocations for a specific UUID."""
    return sum(
        count
        for comp, alloc_uuid, count in _LISTENER_ALLOCATIONS[allocation_key]
        if alloc_uuid == uuid
    )


def _get_allocations_for_uuid(uuid: int | str, event_type: str) -> int:
    """Get total allocated listeners for a specific UUID and event type."""
    if event_type not in ("WRITE", "READ"):
        return 0
    key = f"characteristic_{event_type.lower()}"
    return _sum_allocations_for_uuid(key, uuid)


def _get_descriptor_allocations_for_uuid(uuid: int | str) -> int:
    """Get total allocated listeners for a descriptor UUID."""
    return _sum_allocations_for_uuid("descriptor_write", uuid)


def sanitize_uuid_for_identifier(uuid: int | str) -> str:
    """Convert UUID to valid C++ identifier."""
    if isinstance(uuid, int):
        return f"0x{uuid:04X}"
    # For string UUIDs, replace dashes and colons with underscores
    return str(uuid).replace("-", "_").replace(":", "_").lower()


CONF_ADVERTISE = "advertise"
CONF_APPEARANCE = "appearance"
CONF_BROADCAST = "broadcast"
CONF_CHARACTERISTICS = "characteristics"
CONF_DESCRIPTION = "description"
CONF_DESCRIPTORS = "descriptors"
CONF_ENDIANNESS = "endianness"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_INDICATE = "indicate"
CONF_MANUFACTURER = "manufacturer"
CONF_MANUFACTURER_DATA = "manufacturer_data"
CONF_ON_WRITE = "on_write"
CONF_READ = "read"
CONF_STRING = "string"
CONF_STRING_ENCODING = "string_encoding"
CONF_WRITE = "write"
CONF_WRITE_NO_RESPONSE = "write_no_response"

# Internal configuration keys
CONF_CHAR_VALUE_ACTION_ID_ = "char_value_action_id_"

# BLE reserverd UUIDs
CCCD_DESCRIPTOR_UUID = 0x2902
CUD_DESCRIPTOR_UUID = 0x2901
DEVICE_INFORMATION_SERVICE_UUID = 0x180A
MANUFACTURER_NAME_CHARACTERISTIC_UUID = 0x2A29
MODEL_CHARACTERISTIC_UUID = 0x2A24
FIRMWARE_VERSION_CHARACTERISTIC_UUID = 0x2A26

# Core key to store the global configuration
KEY_NOTIFY_REQUIRED = "notify_required"
KEY_SET_VALUE = "set_value"

esp32_ble_server_ns = cg.esphome_ns.namespace("esp32_ble_server")
ESPBTUUID_ns = cg.esphome_ns.namespace("esp32_ble").namespace("ESPBTUUID")
BLECharacteristic_ns = esp32_ble_server_ns.namespace("BLECharacteristic")
BLEServer = esp32_ble_server_ns.class_(
    "BLEServer",
    cg.Component,
    esp32_ble.GATTsEventHandler,
    cg.Parented.template(esp32_ble.ESP32BLE),
)
esp32_ble_server_automations_ns = esp32_ble_server_ns.namespace(
    "esp32_ble_server_automations"
)
BLETriggers_ns = esp32_ble_server_automations_ns.namespace("BLETriggers")
BLEDescriptor = esp32_ble_server_ns.class_("BLEDescriptor")
BLECharacteristic = esp32_ble_server_ns.class_("BLECharacteristic")
BLEService = esp32_ble_server_ns.class_("BLEService")
BLECharacteristicSetValueAction = esp32_ble_server_automations_ns.class_(
    "BLECharacteristicSetValueAction", automation.Action
)
BLEDescriptorSetValueAction = esp32_ble_server_automations_ns.class_(
    "BLEDescriptorSetValueAction", automation.Action
)
BLECharacteristicNotifyAction = esp32_ble_server_automations_ns.class_(
    "BLECharacteristicNotifyAction", automation.Action
)
bytebuffer_ns = cg.esphome_ns.namespace("bytebuffer")
Endianness_ns = bytebuffer_ns.namespace("Endian")
ByteBuffer_ns = bytebuffer_ns.namespace("ByteBuffer")
ByteBuffer = bytebuffer_ns.class_("ByteBuffer")


PROPERTY_MAP = {
    CONF_READ: BLECharacteristic_ns.PROPERTY_READ,
    CONF_WRITE: BLECharacteristic_ns.PROPERTY_WRITE,
    CONF_NOTIFY: BLECharacteristic_ns.PROPERTY_NOTIFY,
    CONF_BROADCAST: BLECharacteristic_ns.PROPERTY_BROADCAST,
    CONF_INDICATE: BLECharacteristic_ns.PROPERTY_INDICATE,
    CONF_WRITE_NO_RESPONSE: BLECharacteristic_ns.PROPERTY_WRITE_NR,
}


class ValueType:
    def __init__(self, type_, validator, length):
        self.type_ = type_
        self.validator = validator
        self.length = length

    def validate(self, value, encoding):
        value = self.validator(value)
        if self.type_ == "string":
            try:
                value.encode(encoding)
            except UnicodeEncodeError as e:
                raise cv.Invalid(str(e)) from e
        return value


VALUE_TYPES = {
    type_name: ValueType(type_name, validator, length)
    for type_name, validator, length in (
        ("uint8_t", cv.uint8_t, 1),
        ("uint16_t", cv.uint16_t, 2),
        ("uint32_t", cv.uint32_t, 4),
        ("uint64_t", cv.uint64_t, 8),
        ("int8_t", cv.int_range(-128, 127), 1),
        ("int16_t", cv.int_range(-32768, 32767), 2),
        ("int32_t", cv.int_range(-2147483648, 2147483647), 4),
        ("int64_t", cv.int_range(-9223372036854775808, 9223372036854775807), 8),
        ("float", cv.float_, 4),
        ("double", cv.float_, 8),
        ("string", cv.string_strict, None),  # Length is variable
    )
}


def validate_char_on_write(char_config):
    if (
        CONF_ON_WRITE in char_config
        and not char_config[CONF_WRITE]
        and not char_config[CONF_WRITE_NO_RESPONSE]
    ):
        raise cv.Invalid(
            f"{CONF_ON_WRITE} requires the {CONF_WRITE} or {CONF_WRITE_NO_RESPONSE} property to be set"
        )
    return char_config


def validate_descriptor(desc_config):
    if CONF_ON_WRITE in desc_config and not desc_config[CONF_WRITE]:
        raise cv.Invalid(
            f"{CONF_ON_WRITE} requires the {CONF_WRITE} property to be set"
        )
    if CONF_MAX_LENGTH not in desc_config:
        value = desc_config[CONF_VALUE][CONF_DATA]
        if cg.is_template(value):
            raise cv.Invalid(
                f"Descriptor {desc_config[CONF_UUID]} has a templatable value and the {CONF_MAX_LENGTH} property is not set"
            )
        if isinstance(value, list):
            desc_config[CONF_MAX_LENGTH] = len(value)
        elif isinstance(value, str):
            desc_config[CONF_MAX_LENGTH] = len(
                value.encode(desc_config[CONF_VALUE][CONF_STRING_ENCODING])
            )
        else:
            desc_config[CONF_MAX_LENGTH] = VALUE_TYPES[
                desc_config[CONF_VALUE][CONF_TYPE]
            ].length
    return desc_config


def validate_notify_action(config):
    # Store the characteristic ID in the global data for the final validation
    data = CORE.data.setdefault(DOMAIN, {}).setdefault(KEY_NOTIFY_REQUIRED, set())
    data.add(config[CONF_ID])
    return config


def validate_set_value_action(config):
    # Store the characteristic ID in the global data for the final validation
    data = CORE.data.setdefault(DOMAIN, {}).setdefault(KEY_SET_VALUE, set())
    data.add(config[CONF_ID])
    return config


def create_description_cud(char_config):
    if CONF_DESCRIPTION not in char_config:
        return char_config
    # If the config displays a description, there cannot be a descriptor with the CUD UUID
    for desc in char_config[CONF_DESCRIPTORS]:
        if desc[CONF_UUID] == CUD_DESCRIPTOR_UUID:
            raise cv.Invalid(
                f"Characteristic {char_config[CONF_UUID]} has a description, but a CUD descriptor is already present"
            )
    # Manually add the CUD descriptor
    char_config[CONF_DESCRIPTORS].append(
        DESCRIPTOR_SCHEMA(
            {
                CONF_UUID: CUD_DESCRIPTOR_UUID,
                CONF_READ: True,
                CONF_WRITE: False,
                CONF_VALUE: char_config[CONF_DESCRIPTION],
            }
        )
    )
    return char_config


def create_notify_cccd(char_config):
    if not char_config[CONF_NOTIFY] and not char_config[CONF_INDICATE]:
        return char_config
    # If the CCCD descriptor is already present, return the config
    for desc in char_config[CONF_DESCRIPTORS]:
        if desc[CONF_UUID] == CCCD_DESCRIPTOR_UUID:
            # Check if the WRITE property is set
            if not desc[CONF_WRITE]:
                raise cv.Invalid(
                    f"Characteristic {char_config[CONF_UUID]} has notify actions, but the CCCD descriptor does not have the {CONF_WRITE} property set"
                )
            return char_config
    # Manually add the CCCD descriptor
    char_config[CONF_DESCRIPTORS].append(
        DESCRIPTOR_SCHEMA(
            {
                CONF_UUID: CCCD_DESCRIPTOR_UUID,
                CONF_READ: True,
                CONF_WRITE: True,
                CONF_MAX_LENGTH: 2,
                CONF_VALUE: [0, 0],
            }
        )
    )
    return char_config


def create_device_information_service(config):
    # If there is already a device information service,
    # there cannot be CONF_MODEL, CONF_MANUFACTURER or CONF_FIRMWARE_VERSION properties
    for service in config[CONF_SERVICES]:
        if service[CONF_UUID] == DEVICE_INFORMATION_SERVICE_UUID:
            if (
                CONF_MODEL in config
                or CONF_MANUFACTURER in config
                or CONF_FIRMWARE_VERSION in config
            ):
                raise cv.Invalid(
                    "Device information service already present, cannot add manufacturer, model or firmware version"
                )
            return config
    project = CORE.raw_config[CONF_ESPHOME].get(CONF_PROJECT, {})
    model = config.get(CONF_MODEL, project.get("name", CORE.data["esp32"]["board"]))
    version = config.get(
        CONF_FIRMWARE_VERSION, project.get("version", "ESPHome " + ESPHOME_VERSION)
    )
    # Manually add the device information service
    config[CONF_SERVICES].append(
        SERVICE_SCHEMA(
            {
                CONF_UUID: DEVICE_INFORMATION_SERVICE_UUID,
                CONF_CHARACTERISTICS: [
                    {
                        CONF_UUID: MANUFACTURER_NAME_CHARACTERISTIC_UUID,
                        CONF_READ: True,
                        CONF_VALUE: config.get(CONF_MANUFACTURER, "ESPHome"),
                    },
                    {
                        CONF_UUID: MODEL_CHARACTERISTIC_UUID,
                        CONF_READ: True,
                        CONF_VALUE: model,
                    },
                    {
                        CONF_UUID: FIRMWARE_VERSION_CHARACTERISTIC_UUID,
                        CONF_READ: True,
                        CONF_VALUE: version,
                    },
                ],
            }
        )
    )
    return config


def final_validate_config(config):
    # Check if all characteristics that require notifications have the notify property set
    for char_id in CORE.data.get(DOMAIN, {}).get(KEY_NOTIFY_REQUIRED, set()):
        # Look for the characteristic in the configuration
        char_config = [
            char_conf
            for service_conf in config[CONF_SERVICES]
            for char_conf in service_conf[CONF_CHARACTERISTICS]
            if char_conf[CONF_ID] == char_id
        ][0]
        if not char_config[CONF_NOTIFY]:
            raise cv.Invalid(
                f"Characteristic {char_config[CONF_UUID]} has notify actions and the {CONF_NOTIFY} property is not set"
            )
    for char_id in CORE.data.get(DOMAIN, {}).get(KEY_SET_VALUE, set()):
        # Look for the characteristic in the configuration
        char_config = [
            char_conf
            for service_conf in config[CONF_SERVICES]
            for char_conf in service_conf[CONF_CHARACTERISTICS]
            if char_conf[CONF_ID] == char_id
        ][0]
        if isinstance(char_config.get(CONF_VALUE, {}).get(CONF_DATA), cv.Lambda):
            raise cv.Invalid(
                f"Characteristic {char_config[CONF_UUID]} has both a set_value action and a templated value"
            )
    return config


def validate_value_type(value_config):
    # If the value is a not a templatable, the type must be set
    value = value_config[CONF_DATA]

    if type_ := value_config.get(CONF_TYPE):
        if cg.is_template(value):
            raise cv.Invalid(
                f'The "{CONF_TYPE}" property is not allowed for templatable values'
            )
        value_config[CONF_DATA] = VALUE_TYPES[type_].validate(
            value, value_config[CONF_STRING_ENCODING]
        )
    elif isinstance(value, (float, int)):
        raise cv.Invalid(
            f'The "{CONF_TYPE}" property is required for the value "{value}"'
        )
    return value_config


def validate_encoding(value):
    if value == SCHEMA_EXTRACT:
        return cv.one_of("utf-8", "latin-1", "ascii", "utf-16", "utf-32")
    value = encodings.normalize_encoding(value)
    if not value:
        raise cv.Invalid("Invalid encoding")
    return value


def value_schema(default_type=UNDEFINED, templatable=True):
    data_validators = [
        cv.string_strict,
        cv.int_,
        cv.float_,
        cv.All([cv.uint8_t], cv.Length(min=1)),
    ]
    if templatable:
        data_validators.append(cv.returning_lambda)

    return cv.maybe_simple_value(
        cv.All(
            {
                cv.Required(CONF_DATA): cv.Any(*data_validators),
                cv.Optional(CONF_TYPE, default=default_type): cv.one_of(
                    *VALUE_TYPES, lower=True
                ),
                cv.Optional(CONF_STRING_ENCODING, default="utf_8"): validate_encoding,
                cv.Optional(CONF_ENDIANNESS, default="LITTLE"): cv.enum(
                    {
                        "LITTLE": Endianness_ns.LITTLE,
                        "BIG": Endianness_ns.BIG,
                    }
                ),
            },
            validate_value_type,
        ),
        key=CONF_DATA,
    )


DESCRIPTOR_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(BLEDescriptor),
        cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
        cv.Optional(CONF_READ, default=True): cv.boolean,
        cv.Optional(CONF_WRITE, default=True): cv.boolean,
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
        cv.Required(CONF_VALUE): value_schema(templatable=False),
        cv.Optional(CONF_MAX_LENGTH): cv.uint16_t,
    },
    validate_descriptor,
)

CHARACTERISTIC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLECharacteristic),
        cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
        cv.Optional(CONF_VALUE): value_schema(templatable=True),
        cv.GenerateID(CONF_CHAR_VALUE_ACTION_ID_): cv.declare_id(
            BLECharacteristicSetValueAction
        ),
        cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(DESCRIPTOR_SCHEMA),
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
        cv.Optional(CONF_DESCRIPTION): value_schema(
            default_type="string", templatable=False
        ),
    },
    extra_schemas=[
        validate_char_on_write,
        create_description_cud,
        create_notify_cccd,
    ],
).extend({cv.Optional(k, default=False): cv.boolean for k in PROPERTY_MAP})

SERVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEService),
        cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
        cv.Optional(CONF_ADVERTISE, default=False): cv.boolean,
        cv.Optional(CONF_CHARACTERISTICS, default=[]): cv.ensure_list(
            CHARACTERISTIC_SCHEMA
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEServer),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_MANUFACTURER): value_schema("string", templatable=False),
        cv.Optional(CONF_APPEARANCE, default=0): cv.uint16_t,
        cv.Optional(CONF_MODEL): value_schema("string", templatable=False),
        cv.Optional(CONF_FIRMWARE_VERSION): value_schema("string", templatable=False),
        cv.Optional(CONF_MANUFACTURER_DATA): cv.Schema([cv.uint8_t]),
        cv.Optional(CONF_SERVICES, default=[]): cv.ensure_list(SERVICE_SCHEMA),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(single=True),
    },
    extra_schemas=[create_device_information_service],
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = final_validate_config


def parse_properties(char_conf):
    return sum(
        (PROPERTY_MAP[k] for k in char_conf if k in PROPERTY_MAP and char_conf[k]),
        start=0,
    )


def parse_uuid(uuid):
    # If the UUID is a int, use from_uint32
    if isinstance(uuid, int):
        return ESPBTUUID_ns.from_uint32(uuid)
    # Otherwise, use ESPBTUUID_ns.from_raw
    return ESPBTUUID_ns.from_raw(uuid)


async def parse_value(value_config, args):
    value = value_config[CONF_DATA]
    if isinstance(value, cv.Lambda):
        return await cg.templatable(value, args, cg.std_vector.template(cg.uint8))

    if isinstance(value, str):
        value = list(value.encode(value_config[CONF_STRING_ENCODING]))
    if isinstance(value, list):
        return cg.std_vector.template(cg.uint8)(value)
    val = cg.RawExpression(f"{value_config[CONF_TYPE]}({cg.safe_exp(value)})")
    return ByteBuffer_ns.wrap(val, value_config[CONF_ENDIANNESS])


def calculate_num_handles(service_config):
    total = 1 + len(service_config[CONF_CHARACTERISTICS]) * 2
    total += sum(
        len(char_conf[CONF_DESCRIPTORS])
        for char_conf in service_config[CONF_CHARACTERISTICS]
    )
    return total


async def to_code_descriptor(descriptor_conf, char_var):
    value = await parse_value(descriptor_conf[CONF_VALUE], {})
    desc_var = cg.new_Pvariable(
        descriptor_conf[CONF_ID],
        parse_uuid(descriptor_conf[CONF_UUID]),
        descriptor_conf[CONF_MAX_LENGTH],
        descriptor_conf[CONF_READ],
        descriptor_conf[CONF_WRITE],
    )
    cg.add(char_var.add_descriptor(desc_var))
    cg.add(desc_var.set_value(value))
    if CONF_ON_WRITE in descriptor_conf:
        on_write_conf = descriptor_conf[CONF_ON_WRITE]
        cg.add_define("USE_ESP32_BLE_SERVER_DESCRIPTOR_ON_WRITE")
        await automation.build_automation(
            BLETriggers_ns.create_descriptor_on_write_trigger(desc_var),
            [(cg.std_vector.template(cg.uint8), "x"), (cg.uint16, "id")],
            on_write_conf,
        )


async def to_code_characteristic(service_var, char_conf):
    char_var = cg.Pvariable(
        char_conf[CONF_ID],
        service_var.create_characteristic(
            parse_uuid(char_conf[CONF_UUID]),
            parse_properties(char_conf),
        ),
    )

    # If this characteristic has notify or indicate, it will get a CCCD descriptor (0x2902)
    # and ble_characteristic.cpp will register a listener for it
    if char_conf.get(CONF_NOTIFY, False) or char_conf.get(CONF_INDICATE, False):
        allocate_descriptor_event_listener(0x2902, "WRITE", "esp32_ble_server", 1)

    if CONF_ON_WRITE in char_conf:
        on_write_conf = char_conf[CONF_ON_WRITE]
        cg.add_define("USE_ESP32_BLE_SERVER_CHARACTERISTIC_ON_WRITE")
        await automation.build_automation(
            BLETriggers_ns.create_characteristic_on_write_trigger(char_var),
            [(cg.std_vector.template(cg.uint8), "x"), (cg.uint16, "id")],
            on_write_conf,
        )
    if CONF_VALUE in char_conf:
        # Check if the value is templated (Lambda)
        value_data = char_conf[CONF_VALUE][CONF_DATA]
        if isinstance(value_data, cv.Lambda):
            # Templated value - need the full action infrastructure
            action_conf = {
                CONF_ID: char_conf[CONF_ID],
                CONF_VALUE: char_conf[CONF_VALUE],
            }
            value_action = await ble_server_characteristic_set_value(
                action_conf,
                char_conf[CONF_CHAR_VALUE_ACTION_ID_],
                cg.TemplateArguments(),
                {},
            )
            cg.add(value_action.play())
        else:
            # Static value - just set it directly without action infrastructure
            value = await parse_value(char_conf[CONF_VALUE], {})
            cg.add(char_var.set_value(value))
    for descriptor_conf in char_conf[CONF_DESCRIPTORS]:
        await to_code_descriptor(descriptor_conf, char_var)


async def to_code(config):
    # Register the loggers this component needs
    esp32_ble.register_bt_logger(BTLoggers.GATT, BTLoggers.SMP)
    cg.add_define("USE_ESP32_BLE_UUID")

    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(parent.register_gatts_event_handler(var))
    cg.add(parent.register_ble_status_event_handler(var))
    cg.add(var.set_parent(parent))
    cg.add(parent.advertising_set_appearance(config[CONF_APPEARANCE]))
    if CONF_MANUFACTURER_DATA in config:
        cg.add(var.set_manufacturer_data(config[CONF_MANUFACTURER_DATA]))
    for service_config in config[CONF_SERVICES]:
        # Calculate the optimal number of handles based on the number of characteristics and descriptors
        num_handles = calculate_num_handles(service_config)
        service_var = cg.Pvariable(
            service_config[CONF_ID],
            var.create_service(
                parse_uuid(service_config[CONF_UUID]),
                service_config[CONF_ADVERTISE],
                num_handles,
            ),
        )
        for char_conf in service_config[CONF_CHARACTERISTICS]:
            await to_code_characteristic(service_var, char_conf)
        if service_config[CONF_UUID] == DEVICE_INFORMATION_SERVICE_UUID:
            cg.add(var.set_device_information_service(service_var))
        else:
            cg.add(var.enqueue_start_service(service_var))
    if CONF_ON_CONNECT in config:
        cg.add_define("USE_ESP32_BLE_SERVER_ON_CONNECT")
        await automation.build_automation(
            BLETriggers_ns.create_server_on_connect_trigger(var),
            [(cg.uint16, "id")],
            config[CONF_ON_CONNECT],
        )
    if CONF_ON_DISCONNECT in config:
        cg.add_define("USE_ESP32_BLE_SERVER_ON_DISCONNECT")
        await automation.build_automation(
            BLETriggers_ns.create_server_on_disconnect_trigger(var),
            [(cg.uint16, "id")],
            config[CONF_ON_DISCONNECT],
        )

    # Generate defines for BLECharacteristicSetValueActionManager
    set_value_action_count = sum(
        count for comp, count in _LISTENER_ALLOCATIONS["server_connect"]
    )
    if set_value_action_count > 0:
        cg.add_define("BLE_SET_VALUE_ACTION_MAX_LISTENERS", str(set_value_action_count))

    # Generate defines and specialized classes for server events
    server_connect_count = sum(
        count for comp, count in _LISTENER_ALLOCATIONS["server_connect"]
    )
    server_disconnect_count = sum(
        count for comp, count in _LISTENER_ALLOCATIONS["server_disconnect"]
    )

    if server_connect_count > 0 or server_disconnect_count > 0:
        # Generate the specialized BLEServer class with EventEmitter support
        cg.add_define(
            "BLE_SERVER_CONNECT_MAX_LISTENERS", str(max(server_connect_count, 1))
        )
        cg.add_define(
            "BLE_SERVER_DISCONNECT_MAX_LISTENERS",
            str(max(server_disconnect_count, 1)),
        )
        # TODO: Generate specialized BLEServer class with EventEmitter mixins

    # Generate defines and specialized classes for characteristics
    # Group allocations by UUID
    char_write_by_uuid: dict[int | str, int] = {}
    for comp, uuid, count in _LISTENER_ALLOCATIONS["characteristic_write"]:
        char_write_by_uuid[uuid] = char_write_by_uuid.get(uuid, 0) + count

    char_read_by_uuid: dict[int | str, int] = {}
    for comp, uuid, count in _LISTENER_ALLOCATIONS["characteristic_read"]:
        char_read_by_uuid[uuid] = char_read_by_uuid.get(uuid, 0) + count

    # Generate defines for each UUID
    for uuid, count in char_write_by_uuid.items():
        uuid_id = sanitize_uuid_for_identifier(uuid)
        cg.add_define(f"BLE_CHAR_{uuid_id}_WRITE_MAX_LISTENERS", str(count))

    for uuid, count in char_read_by_uuid.items():
        uuid_id = sanitize_uuid_for_identifier(uuid)
        cg.add_define(f"BLE_CHAR_{uuid_id}_READ_MAX_LISTENERS", str(count))

    # Generate defines and specialized classes for descriptors
    descriptor_write_by_uuid: dict[int | str, int] = {}
    for comp, uuid, count in _LISTENER_ALLOCATIONS["descriptor_write"]:
        descriptor_write_by_uuid[uuid] = descriptor_write_by_uuid.get(uuid, 0) + count

    for uuid, count in descriptor_write_by_uuid.items():
        uuid_id = sanitize_uuid_for_identifier(uuid)
        cg.add_define(f"BLE_DESC_{uuid_id}_WRITE_MAX_LISTENERS", str(count))

    # Generate specialized characteristic classes with EventEmitter support
    for uuid in set(list(char_write_by_uuid.keys()) + list(char_read_by_uuid.keys())):
        uuid_id = sanitize_uuid_for_identifier(uuid)
        write_count = char_write_by_uuid.get(uuid, 0)
        read_count = char_read_by_uuid.get(uuid, 0)

        # Generate specialized class that inherits from BLECharacteristic and adds EventEmitter support
        class_name = f"BLECharacteristic_{uuid_id}"

        # Build the class header with appropriate EventEmitter base classes
        base_classes = ["BLECharacteristic"]
        template_params = []

        if write_count > 0:
            base_classes.append(
                f"EventEmitter<BLECharacteristicEvt::SpanEvt, BLE_CHAR_{uuid_id}_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>"
            )
            template_params.append("write")
        if read_count > 0:
            base_classes.append(
                f"EventEmitter<BLECharacteristicEvt::EmptyEvt, BLE_CHAR_{uuid_id}_READ_MAX_LISTENERS, uint16_t>"
            )
            template_params.append("read")

        cg.add_global(
            cg.RawExpression(f"""
class {class_name} : public {", public ".join(base_classes)} {{
 public:
  {class_name}(ESPBTUUID uuid, uint32_t properties, uint16_t max_len = 100)
      : BLECharacteristic(uuid, properties, max_len) {{}}

  // Override virtual methods to provide EventEmitter functionality
  {"EventEmitterListenerID on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&listener) override {" if write_count > 0 else ""}
  {"  return this->EventEmitter<BLECharacteristicEvt::SpanEvt, BLE_CHAR_" + uuid_id + "_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::on(BLECharacteristicEvt::SpanEvt::ON_WRITE, std::move(listener));" if write_count > 0 else ""}
  {"}" if write_count > 0 else ""}

  {"void off_write(EventEmitterListenerID id) override {" if write_count > 0 else ""}
  {"  this->EventEmitter<BLECharacteristicEvt::SpanEvt, BLE_CHAR_" + uuid_id + "_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::off(BLECharacteristicEvt::SpanEvt::ON_WRITE, id);" if write_count > 0 else ""}
  {"}" if write_count > 0 else ""}

  {"EventEmitterListenerID on_read(std::function<void(uint16_t)> &&listener) override {" if read_count > 0 else ""}
  {"  return this->EventEmitter<BLECharacteristicEvt::EmptyEvt, BLE_CHAR_" + uuid_id + "_READ_MAX_LISTENERS, uint16_t>::on(BLECharacteristicEvt::EmptyEvt::ON_READ, std::move(listener));" if read_count > 0 else ""}
  {"}" if read_count > 0 else ""}

  {"void off_read(EventEmitterListenerID id) override {" if read_count > 0 else ""}
  {"  this->EventEmitter<BLECharacteristicEvt::EmptyEvt, BLE_CHAR_" + uuid_id + "_READ_MAX_LISTENERS, uint16_t>::off(BLECharacteristicEvt::EmptyEvt::ON_READ, id);" if read_count > 0 else ""}
  {"}" if read_count > 0 else ""}

 protected:
  {"void emit_on_write_(std::span<const uint8_t> value, uint16_t conn_id) override {" if write_count > 0 else ""}
  {"  this->EventEmitter<BLECharacteristicEvt::SpanEvt, BLE_CHAR_" + uuid_id + "_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::emit_(BLECharacteristicEvt::SpanEvt::ON_WRITE, value, conn_id);" if write_count > 0 else ""}
  {"}" if write_count > 0 else ""}

  {"void emit_on_read_(uint16_t conn_id) override {" if read_count > 0 else ""}
  {"  this->EventEmitter<BLECharacteristicEvt::EmptyEvt, BLE_CHAR_" + uuid_id + "_READ_MAX_LISTENERS, uint16_t>::emit_(BLECharacteristicEvt::EmptyEvt::ON_READ, conn_id);" if read_count > 0 else ""}
  {"}" if read_count > 0 else ""}
}};
""")
        )

    # Generate specialized descriptor classes with EventEmitter support
    for uuid, count in descriptor_write_by_uuid.items():
        uuid_id = sanitize_uuid_for_identifier(uuid)
        class_name = f"BLEDescriptor_{uuid_id}"

        cg.add_global(
            cg.RawExpression(f"""
class {class_name} : public BLEDescriptor,
                     public EventEmitter<BLEDescriptorEvt::SpanEvt, BLE_DESC_{uuid_id}_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t> {{
 public:
  {class_name}(ESPBTUUID uuid, uint16_t max_len = 100, bool read = true, bool write = true)
      : BLEDescriptor(uuid, max_len, read, write) {{}}

  EventEmitterListenerID on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&listener) override {{
    return this->EventEmitter<BLEDescriptorEvt::SpanEvt, BLE_DESC_{uuid_id}_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::on(BLEDescriptorEvt::SpanEvt::ON_WRITE, std::move(listener));
  }}

  void off_write(EventEmitterListenerID id) override {{
    this->EventEmitter<BLEDescriptorEvt::SpanEvt, BLE_DESC_{uuid_id}_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::off(BLEDescriptorEvt::SpanEvt::ON_WRITE, id);
  }}

 protected:
  void emit_on_write_(std::span<const uint8_t> value, uint16_t conn_id) override {{
    this->EventEmitter<BLEDescriptorEvt::SpanEvt, BLE_DESC_{uuid_id}_WRITE_MAX_LISTENERS, std::span<const uint8_t>, uint16_t>::emit_(BLEDescriptorEvt::SpanEvt::ON_WRITE, value, conn_id);
  }}
}};
""")
        )

    # Generate specialized BLEServer class if needed
    if server_connect_count > 0 or server_disconnect_count > 0:
        base_classes = ["BLEServer"]
        if server_connect_count > 0:
            base_classes.append(
                "EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_CONNECT_MAX_LISTENERS, uint16_t>"
            )
        if server_disconnect_count > 0:
            base_classes.append(
                "EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_DISCONNECT_MAX_LISTENERS, uint16_t>"
            )

        cg.add_global(
            cg.RawExpression(f"""
class BLEServerWithEvents : public {", public ".join(base_classes)} {{
 public:
  {"EventEmitterListenerID on_connect(std::function<void(uint16_t)> &&listener) override {" if server_connect_count > 0 else ""}
  {"  return this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_CONNECT_MAX_LISTENERS, uint16_t>::on(BLEServerEvt::EmptyEvt::ON_CONNECT, std::move(listener));" if server_connect_count > 0 else ""}
  {"}" if server_connect_count > 0 else ""}

  {"void off_connect(EventEmitterListenerID id) override {" if server_connect_count > 0 else ""}
  {"  this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_CONNECT_MAX_LISTENERS, uint16_t>::off(BLEServerEvt::EmptyEvt::ON_CONNECT, id);" if server_connect_count > 0 else ""}
  {"}" if server_connect_count > 0 else ""}

  {"EventEmitterListenerID on_disconnect(std::function<void(uint16_t)> &&listener) override {" if server_disconnect_count > 0 else ""}
  {"  return this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_DISCONNECT_MAX_LISTENERS, uint16_t>::on(BLEServerEvt::EmptyEvt::ON_DISCONNECT, std::move(listener));" if server_disconnect_count > 0 else ""}
  {"}" if server_disconnect_count > 0 else ""}

  {"void off_disconnect(EventEmitterListenerID id) override {" if server_disconnect_count > 0 else ""}
  {"  this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_DISCONNECT_MAX_LISTENERS, uint16_t>::off(BLEServerEvt::EmptyEvt::ON_DISCONNECT, id);" if server_disconnect_count > 0 else ""}
  {"}" if server_disconnect_count > 0 else ""}

 protected:
  {"void emit_on_connect_(uint16_t conn_id) override {" if server_connect_count > 0 else ""}
  {"  this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_CONNECT_MAX_LISTENERS, uint16_t>::emit_(BLEServerEvt::EmptyEvt::ON_CONNECT, conn_id);" if server_connect_count > 0 else ""}
  {"}" if server_connect_count > 0 else ""}

  {"void emit_on_disconnect_(uint16_t conn_id) override {" if server_disconnect_count > 0 else ""}
  {"  this->EventEmitter<BLEServerEvt::EmptyEvt, BLE_SERVER_DISCONNECT_MAX_LISTENERS, uint16_t>::emit_(BLEServerEvt::EmptyEvt::ON_DISCONNECT, conn_id);" if server_disconnect_count > 0 else ""}
  {"}" if server_disconnect_count > 0 else ""}
}};
""")
        )

    cg.add_define("USE_ESP32_BLE_SERVER")
    cg.add_define("USE_ESP32_BLE_ADVERTISING")
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)


@automation.register_action(
    "ble_server.characteristic.set_value",
    BLECharacteristicSetValueAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
                cv.Required(CONF_VALUE): value_schema(),
            }
        ),
        validate_set_value_action,
    ),
)
async def ble_server_characteristic_set_value(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    value = await parse_value(config[CONF_VALUE], args)
    cg.add(var.set_buffer(value))
    cg.add_define("USE_ESP32_BLE_SERVER_SET_VALUE_ACTION")
    return var


@automation.register_action(
    "ble_server.descriptor.set_value",
    BLEDescriptorSetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BLEDescriptor),
            cv.Required(CONF_VALUE): value_schema(),
        }
    ),
)
async def ble_server_descriptor_set_value(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    value = await parse_value(config[CONF_VALUE], args)
    cg.add(var.set_buffer(value))
    cg.add_define("USE_ESP32_BLE_SERVER_DESCRIPTOR_SET_VALUE_ACTION")
    return var


@automation.register_action(
    "ble_server.characteristic.notify",
    BLECharacteristicNotifyAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
            }
        ),
        validate_notify_action,
    ),
)
async def ble_server_characteristic_notify(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    cg.add_define("USE_ESP32_BLE_SERVER_NOTIFY_ACTION")
    return cg.new_Pvariable(action_id, template_arg, paren)
