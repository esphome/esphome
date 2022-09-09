import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker, mqtt_room, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    CONF_DISTANCE,
    UNIT_METER,
)

CONF_RSSI = "rssi"
CONF_SIGNAL_POWER = "signal_power"

DEPENDENCIES = ["mqtt_room", "esp32_ble_tracker"]

CODEOWNERS = ["@wjtje"]

ble_room_tracker_ns = cg.esphome_ns.namespace("ble_room_tracker")
BLERoomTracker = ble_room_tracker_ns.class_(
    "BLERoomTracker",
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
    mqtt_room.MqttRoomTracker,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BLERoomTracker),
            cv.Optional(CONF_RSSI): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SIGNAL_POWER): sensor.sensor_schema(
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
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(mqtt_room.MQTT_ROOM_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    await mqtt_room.register_room_tracker(var, config)

    if CONF_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_RSSI])
        cg.add(var.set_rssi_sensor(sens))

    if CONF_SIGNAL_POWER in config:
        sens = await sensor.new_sensor(config[CONF_SIGNAL_POWER])
        cg.add(var.set_signal_power_sensor(sens))

    if CONF_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DISTANCE])
        cg.add(var.set_distance_sensor(sens))
