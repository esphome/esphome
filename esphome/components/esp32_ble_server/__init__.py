import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL
from esphome.components import esp32_ble
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option

AUTO_LOAD = ["esp32_ble"]
CODEOWNERS = ["@jesserockz", "@clydebarrow", "@Rapsssito"]
CONFLICTS_WITH = ["esp32_ble_beacon"]
DEPENDENCIES = ["esp32"]

CONF_MANUFACTURER = "manufacturer"
CONF_MANUFACTURER_DATA = "manufacturer_data"
CONF_SERVICES = "services"
CONF_UUID = "uuid"
CONF_ADVERTISE = "advertise"
CONF_NUM_HANDLES = "num_handles"
CONF_CHARACTERISTICS = "characteristics"
CONF_PROPERTIES = "properties"
CONF_VALUE = "value"
CONF_DESCRIPTORS = "descriptors"
CONF_MAX_LENGTH = "max_length"

esp32_ble_server_ns = cg.esphome_ns.namespace("esp32_ble_server")
ESPBTUUID_ns = cg.esphome_ns.namespace("esp32_ble").namespace("ESPBTUUID")
BLECharacteristic_ns = esp32_ble_server_ns.namespace("BLECharacteristic")
BLEServer = esp32_ble_server_ns.class_(
    "BLEServer",
    cg.Component,
    esp32_ble.GATTsEventHandler,
    cg.Parented.template(esp32_ble.ESP32BLE),
)
BLEDescriptor = esp32_ble_server_ns.class_("BLEDescriptor")
BLECharacteristic = esp32_ble_server_ns.class_("BLECharacteristic")
BLEService = esp32_ble_server_ns.class_("BLEService")


def validate_uuid(value):
    if len(value) != 36:
        raise cv.Invalid("UUID must be exactly 36 characters long")
    return value


# Define a schema for the properties
PROPERTIES_SCHEMA = cv.All(
    cv.ensure_list(cv.one_of(
        "READ",
        "WRITE",
        "NOTIFY",
        "BROADCAST",
        "INDICATE",
        "WRITE_NR",
        upper=True,
    )),
    cv.Length(min=1)
)

UUID_SCHEMA = cv.Any(cv.All(cv.string, validate_uuid), cv.hex_uint32_t)

CHARACTERISTIC_VALUE_SCHEMA = cv.Any(
    cv.All(cv.ensure_list(cv.hex_uint8_t), cv.Length(min=1)),
    cv.string,
    cv.hex_uint8_t,
    cv.hex_uint16_t,
    cv.hex_uint32_t,
    cv.int_,
    cv.float_,
    cv.boolean,
)

SERVICE_CHARACTERISTIC_DESCRIPTOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEDescriptor),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Optional(CONF_MAX_LENGTH, default=0): cv.int_,
        cv.Required(CONF_VALUE): CHARACTERISTIC_VALUE_SCHEMA,
    }
)

SERVICE_CHARACTERISTIC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLECharacteristic),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Required(CONF_PROPERTIES): PROPERTIES_SCHEMA,
        cv.Optional(CONF_VALUE): CHARACTERISTIC_VALUE_SCHEMA,
        cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(SERVICE_CHARACTERISTIC_DESCRIPTOR_SCHEMA),
    }
)

SERVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEService),
        cv.Required(CONF_UUID): UUID_SCHEMA,
        cv.Optional(CONF_ADVERTISE, default=False): cv.boolean,
        cv.Optional(CONF_NUM_HANDLES, default=0): cv.int_,
        cv.Optional(CONF_CHARACTERISTICS, default=[]): cv.ensure_list(SERVICE_CHARACTERISTIC_SCHEMA),
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


def parse_properties(properties):
    result = 0
    for prop in properties:
        if prop == "READ":
            result = result | BLECharacteristic_ns.PROPERTY_READ
        elif prop == "WRITE":
             result = result | BLECharacteristic_ns.PROPERTY_WRITE
        elif prop == "NOTIFY":
             result = result | BLECharacteristic_ns.PROPERTY_NOTIFY
        elif prop == "BROADCAST":
             result = result | BLECharacteristic_ns.PROPERTY_BROADCAST
        elif prop == "INDICATE":
             result = result | BLECharacteristic_ns.PROPERTY_INDICATE
        elif prop == "WRITE_NR":
             result = result | BLECharacteristic_ns.PROPERTY_WRITE_NR
    return result

def parse_uuid(uuid):
    # If the UUID is a string, use from_raw
    if isinstance(uuid, str):
        return ESPBTUUID_ns.from_raw(uuid)
    # Otherwise, use from_uint32
    return ESPBTUUID_ns.from_uint32(uuid)

def parse_value(value):
    if isinstance(value, list):
        return cg.std_vector.template(cg.uint8)(value)
    return value

def calculate_num_handles(service_config):
    total = 1
    for characteristic in service_config[CONF_CHARACTERISTICS]:
        total += 2 # One for the characteristic itself and one for the value
        for _ in characteristic[CONF_DESCRIPTORS]:
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
        service_var = cg.Pvariable(service_config[CONF_ID], var.create_service(
            parse_uuid(service_config[CONF_UUID]),
            service_config[CONF_ADVERTISE],
            num_handles,
        ))
        for characteristic in service_config[CONF_CHARACTERISTICS]:
            char_var = cg.Pvariable(characteristic[CONF_ID], service_var.create_characteristic(
                parse_uuid(characteristic[CONF_UUID]),
                parse_properties(characteristic[CONF_PROPERTIES])
            ))
            if CONF_VALUE in characteristic:
                cg.add(char_var.set_value(parse_value(characteristic[CONF_VALUE])))
            for descriptor in characteristic[CONF_DESCRIPTORS]:
                max_length = descriptor[CONF_MAX_LENGTH]
                # If max_length is 0, calculate the optimal length based on the value
                if max_length == 0:
                    max_length = len(parse_value(descriptor[CONF_VALUE]))
                desc_var = cg.new_Pvariable(descriptor[CONF_ID], parse_uuid(descriptor[CONF_UUID]), max_length)
                if CONF_VALUE in descriptor:
                    cg.add(desc_var.set_value(parse_value(descriptor[CONF_VALUE])))
    cg.add_define("USE_ESP32_BLE_SERVER")
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
