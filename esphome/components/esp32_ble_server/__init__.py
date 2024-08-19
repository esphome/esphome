from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_ble
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_UUID,
    CONF_SERVICES,
    CONF_VALUE,
    CONF_NOTIFY,
)
from esphome.core import CORE

AUTO_LOAD = ["esp32_ble"]
CODEOWNERS = ["@jesserockz", "@clydebarrow", "@Rapsssito"]
DEPENDENCIES = ["esp32"]

CONF_MANUFACTURER = "manufacturer"
CONF_MANUFACTURER_DATA = "manufacturer_data"
CONF_ADVERTISE = "advertise"
CONF_NUM_HANDLES = "num_handles"
CONF_ON_WRITE = "on_write"
CONF_CHARACTERISTICS = "characteristics"
CONF_READ = "read"
CONF_WRITE = "write"
CONF_BROADCAST = "broadcast"
CONF_INDICATE = "indicate"
CONF_WRITE_NO_RESPONSE = "write_no_response"
CONF_DESCRIPTORS = "descriptors"
CONF_VALUE_ACTION_ID_ = "value_action_id_"

# Core key to store the global configuration
_KEY_NOTIFY_REQUIRED = "esp32_ble_server_notify_required"
_KEY_NOTIFY_PROVIDED = "esp32_ble_server_notify_provided"

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
BLECharacteristicNotifyAction = esp32_ble_server_automations_ns.class_(
    "BLECharacteristicNotifyAction", automation.Action
)
ByteBuffer_ns = cg.esphome_ns.namespace("ByteBuffer")
ByteBuffer = cg.esphome_ns.class_("ByteBuffer")


PROPERTY_MAP = {
    CONF_READ: BLECharacteristic_ns.PROPERTY_READ,
    CONF_WRITE: BLECharacteristic_ns.PROPERTY_WRITE,
    CONF_NOTIFY: BLECharacteristic_ns.PROPERTY_NOTIFY,
    CONF_BROADCAST: BLECharacteristic_ns.PROPERTY_BROADCAST,
    CONF_INDICATE: BLECharacteristic_ns.PROPERTY_INDICATE,
    CONF_WRITE_NO_RESPONSE: BLECharacteristic_ns.PROPERTY_WRITE_NR,
}


def validate_uuid(value):
    if len(value) != 36:
        raise cv.Invalid("UUID must be exactly 36 characters long")
    return value


def validate_on_write(char_config):
    if CONF_ON_WRITE in char_config:
        if not char_config[CONF_WRITE] and not char_config[CONF_WRITE_NO_RESPONSE]:
            raise cv.Invalid(
                f"{CONF_ON_WRITE} requires the {CONF_WRITE} or {CONF_WRITE_NO_RESPONSE} property to be set"
            )
    return char_config


def validate_notify_characteristic(char_config):
    if _KEY_NOTIFY_PROVIDED not in CORE.data:
        CORE.data[_KEY_NOTIFY_PROVIDED] = {}
    CORE.data[_KEY_NOTIFY_PROVIDED][char_config[CONF_ID]] = char_config[CONF_NOTIFY]
    # Check if the NOTIFY property is set if the characteristic has a notify action
    char_ids = CORE.data.get(_KEY_NOTIFY_REQUIRED, set())
    if not char_config[CONF_NOTIFY] and char_config[CONF_ID] in char_ids:
        raise cv.Invalid(
            "Characteristic has a notify action, but the NOTIFY property is not set"
        )
    return char_config


def validate_notify_action(action_char_id):
    if _KEY_NOTIFY_REQUIRED not in CORE.data:
        CORE.data[_KEY_NOTIFY_REQUIRED] = set()
    CORE.data[_KEY_NOTIFY_REQUIRED].add(action_char_id)
    # Check if the NOTIFY property is set for the characteristic
    char_notify_value = CORE.data.get(_KEY_NOTIFY_PROVIDED, {}).get(
        action_char_id, None
    )
    if char_notify_value is not None and not char_notify_value:
        raise cv.Invalid(
            "Missing NOTIFY property for characteristic with notify action"
        )
    return action_char_id


UUID_SCHEMA = cv.Any(cv.All(cv.string, validate_uuid), cv.hex_uint32_t)

DESCRIPTOR_VALUE_SCHEMA = cv.Any(
    cv.boolean,
    cv.float_,
    cv.hex_uint8_t,
    cv.hex_uint16_t,
    cv.hex_uint32_t,
    cv.int_,
    cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1)),
    cv.string,
)

CHARACTERISTIC_VALUE_SCHEMA = cv.Any(
    cv.boolean,
    cv.float_,
    cv.hex_uint8_t,
    cv.hex_uint16_t,
    cv.hex_uint32_t,
    cv.int_,
    cv.templatable(cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1))),
    cv.string,
)

DESCRIPTOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEDescriptor),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Required(CONF_VALUE): DESCRIPTOR_VALUE_SCHEMA,
    }
)

SERVICE_CHARACTERISTIC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLECharacteristic),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Optional(CONF_WRITE_NO_RESPONSE, default=False): cv.boolean,
        cv.Optional(CONF_VALUE): CHARACTERISTIC_VALUE_SCHEMA,
        cv.GenerateID(CONF_VALUE_ACTION_ID_): cv.declare_id(
            BLECharacteristicSetValueAction
        ),
        cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(DESCRIPTOR_SCHEMA),
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(
            {cv.GenerateID(): cv.declare_id(BLECharacteristic)}, single=True
        ),
    },
    extra_schemas=[validate_on_write, validate_notify_characteristic],
).extend({cv.Optional(k, default=False): cv.boolean for k in PROPERTY_MAP})

SERVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEService),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Optional(CONF_ADVERTISE, default=False): cv.boolean,
        cv.Optional(CONF_NUM_HANDLES, default=0): cv.int_,
        cv.Optional(CONF_CHARACTERISTICS, default=[]): cv.ensure_list(
            SERVICE_CHARACTERISTIC_SCHEMA
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEServer),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
        cv.Optional(CONF_MANUFACTURER_DATA): cv.Schema([cv.hex_uint8_t]),
        cv.Optional(CONF_MODEL): cv.string,
        cv.Optional(CONF_SERVICES, default=[]): cv.ensure_list(SERVICE_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


def parse_properties(char_conf):
    return sum(
        (PROPERTY_MAP[k] for k in char_conf if k in PROPERTY_MAP and char_conf[k]),
        start=0,
    )


def parse_uuid(uuid):
    # If the UUID is a string, use from_raw
    if isinstance(uuid, str):
        return ESPBTUUID_ns.from_raw(uuid)
    # Otherwise, use from_uint32
    return ESPBTUUID_ns.from_uint32(uuid)


def parse_descriptor_value(value):
    # Compute the maximum length of the descriptor value
    # Also parse the value for byte arrays
    for val_method in [
        cv.boolean,
        cv.float_,
        cv.hex_uint8_t,
        cv.hex_uint16_t,
        cv.hex_uint32_t,
        cv.int_,
        cv.string,
    ]:
        try:
            val = val_method(value)
            buffer = ByteBuffer_ns.wrap(val)
            return buffer, buffer.get_capacity()
        except cv.Invalid:
            pass
    # Assume it's a list of bytes
    try:
        val = cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1))(value)
        buffer = ByteBuffer_ns.wrap(cg.std_vector.template(cg.uint8)(val))
        return buffer, buffer.get_capacity()
    except cv.Invalid:
        pass
    raise cv.Invalid(f"Could not find type for value: {value}")


async def parse_characteristic_value(value, args):
    if isinstance(value, cv.Lambda):
        return await cg.templatable(
            value,
            args,
            ByteBuffer,
            ByteBuffer_ns.wrap,
        )
    for val_method in [
        cv.boolean,
        cv.float_,
        cv.hex_uint8_t,
        cv.hex_uint16_t,
        cv.hex_uint32_t,
        cv.int_,
        cv.string,
    ]:
        try:
            val = val_method(value)
            return ByteBuffer_ns.wrap(val)
        except cv.Invalid:
            pass
    # Assume it's a list of bytes
    try:
        val = cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1))(value)
        return ByteBuffer_ns.wrap(cg.std_vector.template(cg.uint8)(val))
    except cv.Invalid:
        pass
    raise cv.Invalid(f"Could not find type for value: {value}")


def calculate_num_handles(service_config):
    total = 1
    for char_conf in service_config[CONF_CHARACTERISTICS]:
        total += 2  # One for the char_conf itself and one for the value
        for _ in char_conf[CONF_DESCRIPTORS]:
            total += 1
    return total


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(parent.register_gatts_event_handler(var))
    cg.add(parent.register_ble_status_event_handler(var))
    cg.add(var.set_parent(parent))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    if CONF_MANUFACTURER_DATA in config:
        cg.add(var.set_manufacturer_data(config[CONF_MANUFACTURER_DATA]))
    if CONF_MODEL in config:
        cg.add(var.set_model(config[CONF_MODEL]))
    for service_config in config[CONF_SERVICES]:
        num_handles = service_config[CONF_NUM_HANDLES]
        # If num_handles is 0, calculate the optimal number of handles based on the number of characteristics and descriptors
        if num_handles == 0:
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
            char_var = cg.Pvariable(
                char_conf[CONF_ID],
                service_var.create_characteristic(
                    parse_uuid(char_conf[CONF_UUID]),
                    parse_properties(char_conf),
                ),
            )
            if CONF_ON_WRITE in char_conf:
                on_write_conf = char_conf[CONF_ON_WRITE]
                await automation.build_automation(
                    BLETriggers_ns.create_on_write_trigger(char_var),
                    [(cg.std_vector.template(cg.uint8), "x")],
                    on_write_conf,
                )
            if CONF_VALUE in char_conf:
                action_conf = {
                    CONF_ID: char_conf[CONF_ID],
                    CONF_VALUE: char_conf[CONF_VALUE],
                }
                value_action = await ble_server_characteristic_set_value(
                    action_conf,
                    char_conf[CONF_VALUE_ACTION_ID_],
                    cg.TemplateArguments(None),
                    {},
                )
                cg.add(value_action.play())
            for descriptor_conf in char_conf[CONF_DESCRIPTORS]:
                descriptor_value, max_length = parse_descriptor_value(
                    descriptor_conf[CONF_VALUE]
                )
                desc_var = cg.new_Pvariable(
                    descriptor_conf[CONF_ID],
                    parse_uuid(descriptor_conf[CONF_UUID]),
                    max_length,
                )
                if CONF_VALUE in descriptor_conf:
                    cg.add(desc_var.set_value(descriptor_value))
                cg.add(char_var.add_descriptor(desc_var))
        cg.add(var.enqueue_start_service(service_var))
    cg.add_define("USE_ESP32_BLE_SERVER")
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)


@automation.register_action(
    "ble_server.characteristic.set_value",
    BLECharacteristicSetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
            cv.Required(CONF_VALUE): CHARACTERISTIC_VALUE_SCHEMA,
        }
    ),
)
async def ble_server_characteristic_set_value(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    value = await parse_characteristic_value(config[CONF_VALUE], args)
    cg.add(var.set_buffer(value))
    return var


@automation.register_action(
    "ble_server.characteristic.notify",
    BLECharacteristicNotifyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.All(
                cv.use_id(BLECharacteristic), validate_notify_action
            ),
        }
    ),
)
async def ble_server_characteristic_notify(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
