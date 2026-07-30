from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHARACTERISTIC_UUID,
    CONF_NOTIFY,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
)
from esphome.types import ConfigType

from .. import ble_client_ns

DEPENDENCIES = ["ble_client"]

CONF_DESCRIPTOR_UUID = "descriptor_uuid"

CONF_ON_NOTIFY = "on_notify"

adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")

BLETextSensor = ble_client_ns.class_(
    "BLETextSensor",
    text_sensor.TextSensor,
    cg.PollingComponent,
    ble_client.BLEClientNode,
)
BLETextSensorNotifyTrigger = ble_client_ns.class_(
    "BLETextSensorNotifyTrigger", automation.Trigger.template(cg.std_string)
)


def check_descriptor_notify(config: ConfigType) -> ConfigType:
    # Not cv.has_at_most_one_key: that tests key presence, and CONF_NOTIFY carries a
    # default, so it is always present. Only a true value conflicts with a descriptor.
    if config.get(CONF_DESCRIPTOR_UUID) and config.get(CONF_NOTIFY):
        raise cv.Invalid(
            f"'{CONF_DESCRIPTOR_UUID}' cannot be combined with '{CONF_NOTIFY}: true'. "
            "Notifications carry the characteristic value, not a descriptor value, so this "
            f"text sensor would never publish. Remove '{CONF_DESCRIPTOR_UUID}' to receive "
            f"notifications, or set '{CONF_NOTIFY}: false' to read the descriptor."
        )
    return config


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(BLETextSensor)
    .extend(
        {
            cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
            cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_DESCRIPTOR_UUID): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
            cv.Optional(CONF_ON_NOTIFY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BLETextSensorNotifyTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    check_descriptor_notify,
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
    for conf in config.get(CONF_ON_NOTIFY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await ble_client.register_ble_node(trigger, config)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
