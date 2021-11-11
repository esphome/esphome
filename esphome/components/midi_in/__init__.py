import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart, binary_sensor
from esphome.const import CONF_ID, CONF_UART_ID, CONF_TRIGGER_ID

CODEOWNERS = ["@muxa"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]

midi_ns = cg.esphome_ns.namespace("midi")

MidiInComponent = midi_ns.class_("MidiInComponent", cg.Component, uart.UARTDevice)

MidiVoiceMessage = midi_ns.class_("MidiVoiceMessage")

MidiSystemMessage = midi_ns.class_("MidiSystemMessage")

MidiInOnVoiceMessageTrigger = midi_ns.class_(
    "MidiInOnVoiceMessageTrigger", automation.Trigger.template()
)

MidiInOnSystemMessageTrigger = midi_ns.class_(
    "MidiInOnSystemMessageTrigger", automation.Trigger.template()
)

MULTI_CONF = True

CONF_ON_VOICE_MESSAGE = "on_voice_message"
CONF_ON_SYSTEM_MESSAGE = "on_system_message"
CONF_CONNECTED_SENSOR = "connected"
CONF_PLAYBACK_SENSOR = "playback"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MidiInComponent),
            cv.Optional(CONF_ON_VOICE_MESSAGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        MidiInOnVoiceMessageTrigger
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
            cv.Optional(
                CONF_CONNECTED_SENSOR
            ): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
            cv.Optional(
                CONF_PLAYBACK_SENSOR
            ): binary_sensor.BINARY_SENSOR_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
        }
    ).extend(uart.UART_DEVICE_SCHEMA)
)


def validate_uart(config):
    uart.final_validate_device_schema(
        "mini_in", baud_rate=31250, require_rx=False, require_tx=True
    )(config)


FINAL_VALIDATE_SCHEMA = validate_uart


async def to_code(config):

    cg.add_global(midi_ns.using)

    uart_component = await cg.get_variable(config[CONF_UART_ID])
    var = cg.new_Pvariable(config[CONF_ID], uart_component)

    if CONF_ON_VOICE_MESSAGE in config:
        for on_message in config.get(CONF_ON_VOICE_MESSAGE, []):
            message_trigger = cg.new_Pvariable(on_message[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                message_trigger, [(MidiVoiceMessage, "x")], on_message
            )

    if CONF_ON_SYSTEM_MESSAGE in config:
        for on_message in config.get(CONF_ON_SYSTEM_MESSAGE, []):
            message_trigger = cg.new_Pvariable(on_message[CONF_TRIGGER_ID], var)
            await automation.build_automation(
                message_trigger, [(MidiSystemMessage, "x")], on_message
            )

    await cg.register_component(var, config)

    if CONF_CONNECTED_SENSOR in config:
        conf = config[CONF_CONNECTED_SENSOR]
        sens = cg.new_Pvariable(conf[CONF_ID])
        await binary_sensor.register_binary_sensor(sens, conf)
        cg.add(getattr(var, "set_connected_binary_sensor")(sens))

    if CONF_PLAYBACK_SENSOR in config:
        conf = config[CONF_PLAYBACK_SENSOR]
        sens = cg.new_Pvariable(conf[CONF_ID])
        await binary_sensor.register_binary_sensor(sens, conf)
        cg.add(getattr(var, "set_playback_binary_sensor")(sens))

    cg.add(var.dump_config())
