import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ID, CONF_STATE_TOPIC

DEPENDENCIES = ["mqtt"]

CODEOWNERS = ["@wjtje"]

DEPENDENCIES = ["esp32_ble_tracker"]

CONF_ROOM = "room"

mqtt_room_ns = cg.esphome_ns.namespace("mqtt_room")
MqttRoom = mqtt_room_ns.class_(
    "MqttRoom", cg.Component, esp32_ble_tracker.ESPBTDeviceListener
)


def validate_config(config):
    if config[CONF_STATE_TOPIC][-1] == "/":
        raise cv.Invalid(
            f"Topic '{config[CONF_STATE_TOPIC]}' is invalid, it shouldn't end with '/'"
        )

    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MqttRoom),
            cv.Optional(CONF_STATE_TOPIC, "esphome/rooms"): cv.string,
            cv.Required(CONF_ROOM): cv.string,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_topic(f"{config[CONF_STATE_TOPIC]}/{config[CONF_ROOM]}"))
