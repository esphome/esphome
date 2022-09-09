import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_STATE_TOPIC

DEPENDENCIES = ["mqtt"]

CODEOWNERS = ["@wjtje"]

CONF_ROOM = "room"
CONF_MQTT_ROOM_ID = "mqtt_room_id"
CONF_TRACKER_ID = "tracker_id"

mqtt_room_ns = cg.esphome_ns.namespace("mqtt_room")
MqttRoom = mqtt_room_ns.class_("MqttRoom", cg.Component)
MqttRoomTracker = mqtt_room_ns.class_("MqttRoomTracker")


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
    ),
    validate_config,
)

MQTT_ROOM_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MQTT_ROOM_ID): cv.use_id(MqttRoom),
        cv.Required(CONF_TRACKER_ID): cv.string,
        cv.Required(CONF_NAME): cv.string,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_topic(f"{config[CONF_STATE_TOPIC]}/{config[CONF_ROOM]}"))


async def register_room_tracker(var, config):
    paren = await cg.get_variable(config[CONF_MQTT_ROOM_ID])
    cg.add(paren.add_tracker(var))
    cg.add(var.set_id(config[CONF_TRACKER_ID]))
    cg.add(var.set_name(config[CONF_NAME]))

    return var
