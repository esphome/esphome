import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_STATE_TOPIC
from esphome.components.ble_rssi import sensor as ble_rssi_sensor

DEPENDENCIES = ["mqtt", "esp32_ble_tracker"]

CODEOWNERS = ["@wjtje"]

CONF_ROOM = "room"
CONF_TRACKERS = "trackers"
CONF_DEVICE_ID = "device_id"

mqtt_room_ns = cg.esphome_ns.namespace("mqtt_room")
MqttRoom = mqtt_room_ns.class_("MqttRoom", cg.Component)


def validate_config(config):
    sensors = []

    for tracker in config[CONF_TRACKERS]:
        if tracker[CONF_DEVICE_ID] in sensors:
            raise cv.Invalid(
                f"'{tracker[CONF_DEVICE_ID]}' can only be used in one tracker"
            )

        sensors.append(tracker[CONF_DEVICE_ID])

    if config[CONF_STATE_TOPIC][-1] == "/":
        raise cv.Invalid(
            f"Topic '{config[CONF_STATE_TOPIC]}' is invalid, all topics shouldn't end with '/'"
        )

    return config


TRACKER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NAME): cv.string,
        cv.Required(CONF_DEVICE_ID): cv.use_id(ble_rssi_sensor.BLERSSISensor),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MqttRoom),
            cv.Optional(CONF_STATE_TOPIC, "esphome/rooms"): cv.string,
            cv.Required(CONF_ROOM): cv.string,
            cv.Required(CONF_TRACKERS): cv.ensure_list(TRACKER_SCHEMA),
        }
    ),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_topic(f"{config[CONF_STATE_TOPIC]}/{config[CONF_ROOM]}"))

    for tracker in config[CONF_TRACKERS]:
        tracker_sensor = await cg.get_variable(tracker[CONF_DEVICE_ID])

        if CONF_NAME in tracker:
            cg.add(var.add_tracker(tracker_sensor, tracker[CONF_NAME]))
        else:
            cg.add(var.add_tracker(tracker_sensor))

    await cg.register_component(var, config)
