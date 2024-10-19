from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_ble
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32_ble import bt_uuid
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_UUID,
    CONF_SERVICES,
    CONF_VALUE,
    CONF_NOTIFY,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
)
from esphome.core import CORE

AUTO_LOAD = ["esp32_ble"]
CODEOWNERS = ["@jesserockz", "@clydebarrow", "@Rapsssito"]
DEPENDENCIES = ["esp32"]

CONF_BYTE_LENGTH = "byte_length"
CONF_MANUFACTURER = "manufacturer"
CONF_MANUFACTURER_DATA = "manufacturer_data"
CONF_ADVERTISE = "advertise"
CONF_ON_WRITE = "on_write"
CONF_CHARACTERISTICS = "characteristics"
CONF_READ = "read"
CONF_WRITE = "write"
CONF_BROADCAST = "broadcast"
CONF_INDICATE = "indicate"
CONF_WRITE_NO_RESPONSE = "write_no_response"
CONF_DESCRIPTORS = "descriptors"
CONF_STRING_ENCODING = "string_encoding"
CONF_DESCRIPTION = "description"

CONF_CHAR_VALUE_ACTION_ID_ = "characteristic_value_action_id_"
CONF_VALUE_BUFFER_ = "value_buffer_"
CONF_CUD_ID_ = "cud_id_"
CONF_CUD_VALUE_BUFFER_ = "cud_value_buffer_"
CONF_CCCD_ID_ = "cccd_id_"
CONF_CCCD_VALUE_BUFFER_ = "cccd_value_buffer_"

# Core key to store the global configuration
_KEY_NOTIFY_REQUIRED = "esp32_ble_server_notify_required"

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


def validate_char_on_write(char_config):
    if CONF_ON_WRITE in char_config:
        if not char_config[CONF_WRITE] and not char_config[CONF_WRITE_NO_RESPONSE]:
            raise cv.Invalid(
                f"{CONF_ON_WRITE} requires the {CONF_WRITE} or {CONF_WRITE_NO_RESPONSE} property to be set"
            )
    return char_config


def validate_desc_on_write(desc_config):
    if CONF_ON_WRITE in desc_config:
        if not desc_config[CONF_WRITE]:
            raise cv.Invalid(
                f"{CONF_ON_WRITE} requires the {CONF_WRITE} property to be set"
            )
    return desc_config


def validate_notify_action(action_char_id):
    # Store the characteristic ID in the global data for the final validation
    if _KEY_NOTIFY_REQUIRED not in CORE.data:
        CORE.data[_KEY_NOTIFY_REQUIRED] = set()
    CORE.data[_KEY_NOTIFY_REQUIRED].add(action_char_id)
    return action_char_id


def validate_descritor_value_length(descriptor_conf):
    # Check if the value length is specified for the descriptor if the value is a templatable
    if (
        cg.is_template(descriptor_conf[CONF_VALUE])
        and CONF_BYTE_LENGTH not in descriptor_conf
    ):
        raise cv.Invalid(
            f"Descriptor {descriptor_conf[CONF_UUID]} is a templatable value, so the {CONF_BYTE_LENGTH} property must be set"
        )
    return descriptor_conf


def create_description_cud(char_config):
    if CONF_DESCRIPTION not in char_config:
        return char_config
    # If the config displays a description, there cannot be a descriptor with the CUD UUID
    for desc in char_config[CONF_DESCRIPTORS]:
        if desc[CONF_UUID] == 0x2901:
            raise cv.Invalid(
                f"Characteristic {char_config[CONF_UUID]} has a description, but a CUD descriptor is already present"
            )
    # Manually add the CUD descriptor
    char_config[CONF_DESCRIPTORS].append(
        {
            CONF_ID: char_config[CONF_CUD_ID_],
            CONF_UUID: 0x2901,
            CONF_READ: True,
            CONF_WRITE: False,
            CONF_VALUE: char_config[CONF_DESCRIPTION],
            CONF_STRING_ENCODING: char_config[CONF_STRING_ENCODING],
            CONF_VALUE_BUFFER_: char_config[CONF_CUD_VALUE_BUFFER_],
        }
    )
    return char_config


def create_notify_cccd(char_config):
    if not char_config[CONF_NOTIFY] and not char_config[CONF_INDICATE]:
        return char_config
    # If the CCCD descriptor is already present, return the config
    for desc in char_config[CONF_DESCRIPTORS]:
        if desc[CONF_UUID] == 0x2902:
            return
    # Manually add the CCCD descriptor
    char_config[CONF_DESCRIPTORS].append(
        {
            CONF_ID: char_config[CONF_CCCD_ID_],
            CONF_UUID: 0x2902,
            CONF_READ: True,
            CONF_WRITE: True,
            CONF_VALUE: [0, 0],
            CONF_STRING_ENCODING: char_config[CONF_STRING_ENCODING],
            CONF_VALUE_BUFFER_: char_config[CONF_CCCD_VALUE_BUFFER_],
        }
    )
    return char_config


def final_validate_config(config):
    # Check if all characteristics that require notifications have the notify property set
    if _KEY_NOTIFY_REQUIRED in CORE.data:
        for char_id in CORE.data[_KEY_NOTIFY_REQUIRED]:
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
    return config


VALUE_SCHEMA = cv.Any(
    cv.boolean,
    cv.uint8_t,
    cv.uint16_t,
    cv.uint32_t,
    cv.int_,
    cv.float_,
    cv.templatable(cv.All(cv.ensure_list(cv.uint8_t), cv.Length(min=1))),
    cv.string,
)

VALUE_EXTRAS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_STRING_ENCODING, default="utf-8"): cv.string,
        cv.Optional(CONF_BYTE_LENGTH): cv.uint16_t,
        cv.GenerateID(CONF_VALUE_BUFFER_): cv.declare_id(ByteBuffer),
    }
)

DESCRIPTOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEDescriptor),
        cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
        cv.Optional(CONF_READ, default=True): cv.boolean,
        cv.Optional(CONF_WRITE, default=True): cv.boolean,
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
        cv.Required(CONF_VALUE): VALUE_SCHEMA,
    },
    extra_schemas=[validate_descritor_value_length, validate_desc_on_write],
).extend(VALUE_EXTRAS_SCHEMA)

SERVICE_CHARACTERISTIC_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLECharacteristic),
            cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
            cv.Optional(CONF_VALUE): VALUE_SCHEMA,
            cv.GenerateID(CONF_CHAR_VALUE_ACTION_ID_): cv.declare_id(
                BLECharacteristicSetValueAction
            ),
            cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(
                DESCRIPTOR_SCHEMA
            ),
            cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
            cv.Optional(CONF_DESCRIPTION): cv.string,
            cv.GenerateID(CONF_CUD_ID_): cv.declare_id(BLEDescriptor),
            cv.GenerateID(CONF_CUD_VALUE_BUFFER_): cv.declare_id(ByteBuffer),
            cv.GenerateID(CONF_CCCD_ID_): cv.declare_id(BLEDescriptor),
            cv.GenerateID(CONF_CCCD_VALUE_BUFFER_): cv.declare_id(ByteBuffer),
        },
        extra_schemas=[
            validate_char_on_write,
            create_description_cud,
            create_notify_cccd,
        ],
    )
    .extend({cv.Optional(k, default=False): cv.boolean for k in PROPERTY_MAP})
    .extend(VALUE_EXTRAS_SCHEMA)
)

SERVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEService),
        cv.Required(CONF_UUID): cv.Any(bt_uuid, cv.hex_uint32_t),
        cv.Optional(CONF_ADVERTISE, default=False): cv.boolean,
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
        cv.Optional(CONF_MANUFACTURER_DATA): cv.Schema([cv.uint8_t]),
        cv.Optional(CONF_MODEL): cv.string,
        cv.Optional(CONF_SERVICES, default=[]): cv.ensure_list(SERVICE_SCHEMA),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(single=True),
    }
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


def bytebuffer_parser_(value, str_encoding):
    for val_method, casting in zip(
        (
            cv.boolean,
            cv.uint8_t,
            cv.uint16_t,
            cv.uint32_t,
            cv.int_,
            cv.float_,
            cv.string,
            cv.All(cv.ensure_list(cv.uint8_t), cv.Length(min=1)),
        ),
        (
            cg.bool_,
            cg.uint8,
            cg.uint16,
            cg.uint32,
            cg.int_,
            cg.float_,
            None,
            cg.std_vector.template(cg.uint8),
        ),
    ):
        try:
            val = val_method(value)
            if val_method == cv.string:
                # Convert to a list of bytes using encoding
                val = cg.std_vector.template(cg.uint8)(list(val.encode(str_encoding)))
            else:
                val = casting(val)
            return val, val_method
        except cv.Invalid:
            pass
    raise cv.Invalid(f"Could not find type for value: {value}")


async def parse_value(value, str_encoding, buffer_id, args, byte_length=None):
    if isinstance(value, cv.Lambda):
        return await cg.templatable(
            value,
            args,
            ByteBuffer,
            ByteBuffer_ns.wrap,
        )

    val, val_method = bytebuffer_parser_(value, str_encoding)
    if byte_length is None:
        # If no byte length is specified, use the default length
        buffer_var = cg.variable(buffer_id, ByteBuffer_ns.wrap(val))
    else:
        put_method_dict = {
            cv.boolean: "put_bool",
            cv.uint8_t: "put_uint8",
            cv.uint16_t: "put_uint16",
            cv.uint32_t: "put_uint32",
            cv.int_: "put_int",
            cv.float_: "put_float",
        }
        # Create a buffer with the specified length and add the value
        put_method = put_method_dict.get(val_method, "put_vector")
        buffer_var = cg.variable(buffer_id, ByteBuffer(byte_length))
        cg.add(getattr(buffer_var, put_method)(val))
    return buffer_var


def calculate_num_handles(service_config):
    total = 1
    for char_conf in service_config[CONF_CHARACTERISTICS]:
        total += 2  # One for the char_conf itself and one for the value
        for _ in char_conf[CONF_DESCRIPTORS]:
            total += 1
    return total


async def to_code_descriptor(descriptor_conf, char_var):
    value = await parse_value(
        descriptor_conf[CONF_VALUE],
        descriptor_conf[CONF_STRING_ENCODING],
        descriptor_conf[CONF_VALUE_BUFFER_],
        {},
        descriptor_conf.get(CONF_BYTE_LENGTH, None),
    )
    desc_var = cg.new_Pvariable(
        descriptor_conf[CONF_ID],
        parse_uuid(descriptor_conf[CONF_UUID]),
        value.get_capacity(),
        descriptor_conf[CONF_READ],
        descriptor_conf[CONF_WRITE],
    )
    cg.add(char_var.add_descriptor(desc_var))
    cg.add(desc_var.set_value(value))
    if CONF_ON_WRITE in descriptor_conf:
        on_write_conf = descriptor_conf[CONF_ON_WRITE]
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
    if CONF_ON_WRITE in char_conf:
        on_write_conf = char_conf[CONF_ON_WRITE]
        await automation.build_automation(
            BLETriggers_ns.create_characteristic_on_write_trigger(char_var),
            [(cg.std_vector.template(cg.uint8), "x"), (cg.uint16, "id")],
            on_write_conf,
        )
    if CONF_VALUE in char_conf:
        action_conf = {
            CONF_ID: char_conf[CONF_ID],
            CONF_VALUE: char_conf[CONF_VALUE],
            CONF_BYTE_LENGTH: char_conf.get(CONF_BYTE_LENGTH, None),
            CONF_STRING_ENCODING: char_conf[CONF_STRING_ENCODING],
            CONF_VALUE_BUFFER_: char_conf[CONF_VALUE_BUFFER_],
        }
        value_action = await ble_server_characteristic_set_value(
            action_conf,
            char_conf[CONF_CHAR_VALUE_ACTION_ID_],
            cg.TemplateArguments(None),
            {},
        )
        cg.add(value_action.play())
    for descriptor_conf in char_conf[CONF_DESCRIPTORS]:
        await to_code_descriptor(descriptor_conf, char_var)


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
        cg.add(var.enqueue_start_service(service_var))
    if CONF_ON_CONNECT in config:
        await automation.build_automation(
            BLETriggers_ns.create_server_on_connect_trigger(var),
            [(cg.uint16, "id")],
            config[CONF_ON_CONNECT],
        )
    if CONF_ON_DISCONNECT in config:
        await automation.build_automation(
            BLETriggers_ns.create_server_on_disconnect_trigger(var),
            [(cg.uint16, "id")],
            config[CONF_ON_DISCONNECT],
        )
    cg.add_define("USE_ESP32_BLE_SERVER")
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)


@automation.register_action(
    "ble_server.characteristic.set_value",
    BLECharacteristicSetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
            cv.Required(CONF_VALUE): VALUE_SCHEMA,
        }
    ).extend(VALUE_EXTRAS_SCHEMA),
)
async def ble_server_characteristic_set_value(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    value = await parse_value(
        config[CONF_VALUE],
        config[CONF_STRING_ENCODING],
        config[CONF_VALUE_BUFFER_],
        args,
        config.get(CONF_BYTE_LENGTH, None),
    )
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
