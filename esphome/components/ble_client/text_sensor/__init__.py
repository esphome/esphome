from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker, text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHARACTERISTIC_UUID, CONF_NOTIFY, CONF_SERVICE_UUID

from .. import (
    CONF_DESCRIPTOR_UUID,
    CONF_ON_NOTIFY,
    ble_client_ns,
    notify_from_on_notify,
    validate_descriptor_not_notify,
)

DEPENDENCIES = ["ble_client"]


adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")

BLETextSensor = ble_client_ns.class_(
    "BLETextSensor",
    text_sensor.TextSensor,
    cg.PollingComponent,
    ble_client.BLEClientNode,
)

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(BLETextSensor)
    .extend(
        {
            cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
            cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_DESCRIPTOR_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
            cv.Optional(CONF_ON_NOTIFY): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    validate_descriptor_not_notify,
    notify_from_on_notify,
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    if len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_service_uuid16(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(
            var.set_service_uuid32(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(config[CONF_SERVICE_UUID])
        cg.add(var.set_service_uuid128(uuid128))

    if len(config[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_char_uuid16(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid32_format
    ):
        cg.add(
            var.set_char_uuid32(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid128_format
    ):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(
            config[CONF_CHARACTERISTIC_UUID]
        )
        cg.add(var.set_char_uuid128(uuid128))

    if descriptor_uuid := config.get(CONF_DESCRIPTOR_UUID):
        if len(descriptor_uuid) == len(esp32_ble_tracker.bt_uuid16_format):
            cg.add(var.set_descr_uuid16(esp32_ble_tracker.as_hex(descriptor_uuid)))
        elif len(descriptor_uuid) == len(esp32_ble_tracker.bt_uuid32_format):
            cg.add(var.set_descr_uuid32(esp32_ble_tracker.as_hex(descriptor_uuid)))
        elif len(descriptor_uuid) == len(esp32_ble_tracker.bt_uuid128_format):
            uuid128 = esp32_ble_tracker.as_reversed_hex_array(descriptor_uuid)
            cg.add(var.set_descr_uuid128(uuid128))

    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_enable_notify(config[CONF_NOTIFY]))
    if on_notify := config.get(CONF_ON_NOTIFY):
        cg.add_define("USE_BLE_TEXT_SENSOR_ON_NOTIFY")
        for conf in on_notify:
            await automation.build_callback_automation(
                var, "add_on_notify_callback", [(cg.std_string, "x")], conf
            )
