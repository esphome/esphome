import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker, sensor
from esphome.const import (
    CONF_DISTANCE,
    CONF_ID,
    CONF_NAME,
    CONF_STATE_TOPIC,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_METER,
)

DEPENDENCIES = ["mqtt", "esp32_ble_tracker"]

CODEOWNERS = ["@wjtje"]

CONF_ROOM = "room"
CONF_TRACKERS = "trackers"
CONF_DEVICE_ID = "device_id"
CONF_SIGNAL_POWER = "signal_power"
CONF_RSSI = "rssi"

mqtt_room_ns = cg.esphome_ns.namespace("mqtt_room")
MqttRoom = mqtt_room_ns.class_(
    "MqttRoom", cg.Component, esp32_ble_tracker.ESPBTDeviceListener
)
MqttRoomTracker = mqtt_room_ns.class_("MqttRoomTracker", cg.Component)


def validate_config(config):
    if config[CONF_STATE_TOPIC][-1] == "/":
        raise cv.Invalid(
            f"Topic '{config[CONF_STATE_TOPIC]}' is invalid, it shouldn't end with '/'"
        )

    return config


TRACKER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MqttRoomTracker),
        cv.Required(CONF_DEVICE_ID): cv.string,
        cv.Optional(CONF_NAME): cv.string,
        cv.Optional(CONF_SIGNAL_POWER, -72): cv.int_range(-100, 0),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MqttRoom),
            cv.Optional(CONF_STATE_TOPIC, "esphome/rooms"): cv.string,
            cv.Required(CONF_ROOM): cv.string,
            cv.Optional(CONF_TRACKERS): cv.ensure_list(TRACKER_SCHEMA),
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

    for tracker_config in config[CONF_TRACKERS]:
        tracker = cg.new_Pvariable(tracker_config[CONF_ID])
        await cg.register_component(tracker, tracker_config)

        cg.add(tracker.set_device_id(tracker_config[CONF_DEVICE_ID]))
        cg.add(tracker.set_signal_power(tracker_config[CONF_SIGNAL_POWER]))

        if CONF_NAME in tracker_config:
            cg.add(tracker.set_name(tracker_config[CONF_NAME]))

        if CONF_RSSI in tracker_config:
            sens = await sensor.new_sensor(tracker_config[CONF_RSSI])
            cg.add(tracker.set_rssi_sensor(sens))

        if CONF_DISTANCE in tracker_config:
            sens = await sensor.new_sensor(tracker_config[CONF_DISTANCE])
            cg.add(tracker.set_distance_sensor(sens))

        cg.add(var.add_tracker(tracker))
