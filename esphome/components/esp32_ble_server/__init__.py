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
_ble_server_config = None


def validate_uuid(value):
    if len(value) != 36:
        raise cv.Invalid("UUID must be exactly 36 characters long")
    return value


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
        cv.Optional(CONF_READ, default=False): cv.boolean,
        cv.Optional(CONF_WRITE, default=False): cv.boolean,
        cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
        cv.Optional(CONF_BROADCAST, default=False): cv.boolean,
        cv.Optional(CONF_INDICATE, default=False): cv.boolean,
        cv.Optional(CONF_WRITE_NO_RESPONSE, default=False): cv.boolean,
        cv.Optional(CONF_VALUE): CHARACTERISTIC_VALUE_SCHEMA,
        cv.GenerateID(CONF_VALUE_ACTION_ID_): cv.declare_id(
            BLECharacteristicSetValueAction
        ),
        cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(DESCRIPTOR_SCHEMA),
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(
            {cv.GenerateID(): cv.declare_id(BLECharacteristic)}, single=True
        ),
    }
)

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
    result = 0
    if char_conf[CONF_READ]:
        result = result | BLECharacteristic_ns.PROPERTY_READ
    if char_conf[CONF_WRITE]:
        result = result | BLECharacteristic_ns.PROPERTY_WRITE
    if char_conf[CONF_NOTIFY]:
        result = result | BLECharacteristic_ns.PROPERTY_NOTIFY
    if char_conf[CONF_BROADCAST]:
        result = result | BLECharacteristic_ns.PROPERTY_BROADCAST
    if char_conf[CONF_INDICATE]:
        result = result | BLECharacteristic_ns.PROPERTY_INDICATE
    if char_conf[CONF_WRITE_NO_RESPONSE]:
        result = result | BLECharacteristic_ns.PROPERTY_WRITE_NR
    return result


def parse_uuid(uuid):
    # If the UUID is a string, use from_raw
    if isinstance(uuid, str):
        return ESPBTUUID_ns.from_raw(uuid)
    # Otherwise, use from_uint32
    return ESPBTUUID_ns.from_uint32(uuid)


def parse_descriptor_value(value):
    # Compute the maximum length of the descriptor value
    # Also parse the value for byte arrays
    try:
        cv.boolean(value)
        return value, 1
    except cv.Invalid:
        pass
    try:
        cv.float_(value)
        return value, 8
    except cv.Invalid:
        pass
    try:
        cv.hex_uint8_t(value)
        return value, 1
    except cv.Invalid:
        pass
    try:
        cv.hex_uint16_t(value)
        return value, 2
    except cv.Invalid:
        pass
    try:
        cv.hex_uint32_t(value)
        return value, 4
    except cv.Invalid:
        pass
    try:
        cv.int_(value)
        return value, 4
    except cv.Invalid:
        pass
    try:
        cv.string(value)
        # Count the bytes in the string
        value_len = len(value.encode("utf-8"))
        return value, value_len
    except cv.Invalid:
        pass
    return (
        cg.std_vector.template(cg.uint8)(value),
        len(value),
    )  # Assume it's a list of bytes


def calculate_num_handles(service_config):
    total = 1
    for char_conf in service_config[CONF_CHARACTERISTICS]:
        total += 2  # One for the char_conf itself and one for the value
        for _ in char_conf[CONF_DESCRIPTORS]:
            total += 1
    return total


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    global _ble_server_config
    _ble_server_config = config

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
                if not char_conf[CONF_WRITE] and not char_conf[CONF_WRITE_NO_RESPONSE]:
                    raise cv.Invalid(
                        f"on_write requires the {CONF_WRITE} or {CONF_WRITE_NO_RESPONSE} property to be set"
                    )
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


async def parse_characteristic_value(value, args):
    if isinstance(value, cv.Lambda):
        return await cg.templatable(
            value,
            args,
            cg.std_vector.template(cg.uint8),
            cg.std_vector.template(cg.uint8),
        )
    if isinstance(value, list):
        return cg.std_vector.template(cg.uint8)(value)
    # Transform the value into a vector of bytes
    try:
        bool_value = cv.boolean(value)
        return cg.RawExpression(f"to_vector({'true' if bool_value else 'false'})")
    except cv.Invalid:
        pass
    try:
        int_ = cv.uint64_t(value)
        return cg.RawExpression(f"to_vector({int_})")
    except cv.Invalid:
        pass
    try:
        float_ = cv.float_(value)
        return cg.RawExpression(f"to_vector({float_})")
    except cv.Invalid:
        pass
    try:
        string_ = cv.string(value)
        return cg.RawExpression(f'to_vector("{string_}")')
    except cv.Invalid:
        pass
    raise cv.Invalid(f"Could not find type for value: {value}")


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
    cg.add(var.set_value(value))
    return var


@automation.register_action(
    "ble_server.characteristic.notify",
    BLECharacteristicNotifyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
        }
    ),
)
async def ble_server_characteristic_notify(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    # Check if the NOTIFY property is set from the global configuration
    assert _ble_server_config is not None
    for service_config in _ble_server_config[CONF_SERVICES]:
        for char_conf in service_config[CONF_CHARACTERISTICS]:
            if char_conf[CONF_ID] == config[CONF_ID]:
                if not char_conf[CONF_NOTIFY]:
                    raise cv.Invalid(
                        f'Characteristic "{char_conf[CONF_ID]}" does not have the NOTIFY property set'
                    )
                break
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
