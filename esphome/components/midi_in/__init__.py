import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart, binary_sensor
from esphome.const import CONF_ID, CONF_UART_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@muxa"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]

midi_ns = cg.esphome_ns.namespace("midi_in")

MidiInComponent = midi_ns.class_("MidiInComponent", cg.Component, uart.UARTDevice)

MidiChannelMessage = midi_ns.class_("MidiChannelMessage")

MidiSystemMessage = midi_ns.class_("MidiSystemMessage")

MidiInOnChannelMessageTrigger = midi_ns.class_(
    "MidiInOnChannelMessageTrigger", automation.Trigger.template()
)

MidiInOnSystemMessageTrigger = midi_ns.class_(
    "MidiInOnSystemMessageTrigger", automation.Trigger.template()
)

MULTI_CONF = True

CONF_CHANNEL = "channel"
CONF_ON_CHANNEL_MESSAGE = "on_channel_message"
CONF_ON_SYSTEM_MESSAGE = "on_system_message"
CONF_CONNECTED = "connected"
CONF_PLAYBACK = "playback"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MidiInComponent),
            cv.Optional(CONF_CHANNEL, default=1): cv.int_range(1, 16),
            cv.Optional(CONF_ON_CHANNEL_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MidiInOnChannelMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_SYSTEM_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MidiInOnSystemMessageTrigger
                    ),
                }
            ),
            cv.Optional(CONF_CONNECTED): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
            cv.Optional(CONF_PLAYBACK): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA),
    cv.only_with_arduino,
)


def validate_uart(config):
    uart.final_validate_device_schema(
        "mini_in", baud_rate=31250, require_rx=False, require_tx=True
    )(config)


FINAL_VALIDATE_SCHEMA = validate_uart


async def to_code(config):

    cg.add_global(midi_ns.using)

    cg.add_library("MIDI Library", "5.0.2")

    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)

    if CONF_ON_CHANNEL_MESSAGE in config:
        for on_message in config.get(CONF_ON_CHANNEL_MESSAGE, []):
            message_trigger = cg.new_Pvariable(on_message[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                message_trigger, [(MidiChannelMessage, "x")], on_message
            )

    if CONF_ON_SYSTEM_MESSAGE in config:
        for on_message in config.get(CONF_ON_SYSTEM_MESSAGE, []):
            message_trigger = cg.new_Pvariable(on_message[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                message_trigger, [(MidiSystemMessage, "x")], on_message
            )

    await cg.register_component(var, config)

    if CONF_CHANNEL in config:
        cg.add(var.set_channel(config[CONF_CHANNEL]))

    if CONF_CONNECTED in config:
        conf = config[CONF_CONNECTED]
        sens = cg.new_Pvariable(conf[CONF_ID])
        await binary_sensor.register_binary_sensor(sens, conf)
        cg.add(getattr(var, "set_connected_binary_sensor")(sens))

    if CONF_PLAYBACK in config:
        conf = config[CONF_PLAYBACK]
        sens = cg.new_Pvariable(conf[CONF_ID])
        await binary_sensor.register_binary_sensor(sens, conf)
        cg.add(getattr(var, "set_playback_binary_sensor")(sens))

    cg.add(var.dump_config())
