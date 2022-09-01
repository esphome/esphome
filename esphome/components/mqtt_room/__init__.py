import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_SENSOR, CONF_TOPIC
from esphome.components.ble_rssi import sensor as ble_rssi_sensor

DEPENDENCIES = ["mqtt", "esp32_ble_tracker"]

CODEOWNERS = ["@wjtje"]

CONF_ROOM = "room"
CONF_TRACKERS = "trackers"

mqtt_room_ns = cg.esphome_ns.namespace("mqtt_room")
MqttRoom = mqtt_room_ns.class_("MqttRoom", cg.Component)


def validate_config(config):
    sensors = []

    for tracker in config[CONF_TRACKERS]:
        if tracker[CONF_SENSOR] in sensors:
            raise cv.Invalid(
                f"'{tracker[CONF_SENSOR]}' can only be used in one tracker"
            )

        sensors.append(tracker[CONF_SENSOR])

    if config[CONF_TOPIC][-1] != "/":
        raise cv.Invalid(
            f"Topic '{config[CONF_TOPIC]}' is invalid, all topics should end with '/'"
        )

    return config


TRACKER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NAME): cv.string,
        cv.Required(CONF_SENSOR): cv.use_id(ble_rssi_sensor.BLERSSISensor),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MqttRoom),
            cv.Optional(CONF_TOPIC, "esphome/rooms/"): cv.string,
            cv.Required(CONF_ROOM): cv.string,
            cv.Required(CONF_TRACKERS): cv.ensure_list(TRACKER_SCHEMA),
        }
    ),
    validate_config,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_topic(config[CONF_TOPIC] + config[CONF_ROOM]))

    for tracker in config[CONF_TRACKERS]:
        tracker_sensor = await cg.get_variable(tracker[CONF_SENSOR])
        tracker_sensor_id = str(tracker_sensor.base)

        if CONF_NAME in tracker:
            cg.add(
                var.add_tracker(tracker_sensor, tracker_sensor_id, tracker[CONF_NAME])
            )
        else:
            cg.add(
                var.add_tracker(tracker_sensor, tracker_sensor_id, tracker_sensor_id)
            )

    await cg.register_component(var, config)
