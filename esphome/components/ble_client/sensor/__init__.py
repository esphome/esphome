from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_client, esp32_ble_tracker, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHARACTERISTIC_UUID,
    CONF_LAMBDA,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import ble_client_ns

DEPENDENCIES = ["ble_client"]

CONF_DESCRIPTOR_UUID = "descriptor_uuid"

CONF_NOTIFY = "notify"
CONF_ON_NOTIFY = "on_notify"
TYPE_CHARACTERISTIC = "characteristic"
TYPE_RSSI = "rssi"

adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")

BLESensor = ble_client_ns.class_(
    "BLESensor", sensor.Sensor, cg.PollingComponent, ble_client.BLEClientNode
)
BLESensorNotifyTrigger = ble_client_ns.class_(
    "BLESensorNotifyTrigger", automation.Trigger.template(cg.float_)
)

BLEClientRssiSensor = ble_client_ns.class_(
    "BLEClientRSSISensor", sensor.Sensor, cg.PollingComponent, ble_client.BLEClientNode
)


def checkType(value):
    if CONF_TYPE not in value and CONF_SERVICE_UUID in value:
        raise cv.Invalid(
            "Looks like you're trying to create a ble characteristic sensor. Please add `type: characteristic` to your sensor config."
        )
    return value


CONFIG_SCHEMA = cv.All(
    checkType,
    cv.typed_schema(
        {
            TYPE_CHARACTERISTIC: sensor.sensor_schema(
                BLESensor,
                accuracy_decimals=0,
            )
            .extend(cv.polling_component_schema("60s"))
            .extend(ble_client.BLE_CLIENT_SCHEMA)
            .extend(
                {
                    cv.Required(CONF_SERVICE_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Required(CONF_CHARACTERISTIC_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Optional(CONF_DESCRIPTOR_UUID): esp32_ble_tracker.bt_uuid,
                    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
                    cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
                    cv.Optional(CONF_ON_NOTIFY): automation.validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                                BLESensorNotifyTrigger
                            ),
                        }
                    ),
                }
            ),
            TYPE_RSSI: sensor.sensor_schema(
                BLEClientRssiSensor,
                accuracy_decimals=0,
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            .extend(cv.polling_component_schema("60s"))
            .extend(ble_client.BLE_CLIENT_SCHEMA),
        },
        lower=True,
    ),
)


async def rssi_sensor_to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)


async def characteristic_sensor_to_code(config):
    var = await sensor.new_sensor(config)
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

    if lambda_config := config.get(CONF_LAMBDA):
        lambda_ = await cg.process_lambda(
            lambda_config, [(adv_data_t_const_ref, "x")], return_type=cg.float_
        )
        cg.add(var.set_data_to_value(lambda_))

    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    cg.add(var.set_enable_notify(config[CONF_NOTIFY]))
    for conf in config.get(CONF_ON_NOTIFY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await ble_client.register_ble_node(trigger, config)
        await automation.build_automation(trigger, [(float, "x")], conf)


async def to_code(config):
    if config[CONF_TYPE] == TYPE_RSSI:
        await rssi_sensor_to_code(config)
    elif config[CONF_TYPE] == TYPE_CHARACTERISTIC:
        await characteristic_sensor_to_code(config)
