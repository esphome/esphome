import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_COMMAND, CONF_PAYLOAD
import esphome.final_validate as fv

from .. import (
    COMMAND_FORMAT_SCHEMA,
    CONF_COMMAND_ENDIAN,
    CONF_COMMAND_FORMAT,
    CONF_COMMAND_REPEAT,
    CONF_COMMAND_SIZE,
    CONF_FRAME_TYPE,
    CONF_POSTAMBLE,
    CONF_PREAMBLE,
    CONF_RS485_FRAME_ID,
    MAX_FRAME_LENGTH_UPPER,
    RS485FrameHub,
    rs485_frame_ns,
    validate_byte,
    validate_frame_type,
    validate_u32,
)

AUTO_LOAD = ["rs485_frame"]

RS485FrameButton = rs485_frame_ns.class_("RS485FrameButton", button.Button)


def _validate_button(config):
    has_command = CONF_COMMAND in config
    has_frame_type = CONF_FRAME_TYPE in config
    has_payload = CONF_PAYLOAD in config
    has_cmd_format = CONF_COMMAND_FORMAT in config

    if has_cmd_format and not has_command:
        raise cv.Invalid(
            "rs485_frame button: 'command_format' requires 'command'. "
            "Per-button command_format is only meaningful with the 'command:' form."
        )
    if has_cmd_format and (has_frame_type or has_payload):
        raise cv.Invalid(
            "rs485_frame button: 'command_format' cannot be combined with 'frame_type'/'payload'."
        )
    if has_command and (has_frame_type or has_payload):
        raise cv.Invalid(
            "rs485_frame button: 'command' is mutually exclusive with 'frame_type'/'payload'. "
            "Use 'command' to encode via the hub's command_format, or use 'frame_type' + "
            "'payload' to emit a raw frame."
        )
    if has_frame_type != has_payload:
        raise cv.Invalid(
            "rs485_frame button raw form requires both 'frame_type' and 'payload'."
        )
    if not has_command and not has_frame_type:
        raise cv.Invalid(
            "rs485_frame button requires either 'command' or 'frame_type' + 'payload'."
        )
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(RS485FrameButton).extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
            cv.Optional(CONF_COMMAND): validate_u32,
            cv.Optional(CONF_FRAME_TYPE): validate_frame_type,
            # Payload is bounded so a button can never assemble a frame larger than the
            # widest hub buffer (max_frame_length is capped at MAX_FRAME_LENGTH_UPPER). The
            # hub also drops at runtime any frame that exceeds its own max_frame_length.
            cv.Optional(CONF_PAYLOAD): cv.All(
                cv.ensure_list(validate_byte), cv.Length(max=MAX_FRAME_LENGTH_UPPER)
            ),
            cv.Optional(CONF_COMMAND_FORMAT): COMMAND_FORMAT_SCHEMA,
        }
    ),
    _validate_button,
)


def _final_validate(config):
    # The `command:` form requires a command_format somewhere — either on the button itself
    # or on the hub. If the button supplies its own command_format, skip the hub check.
    if CONF_COMMAND not in config:
        return config
    if CONF_COMMAND_FORMAT in config:
        return config

    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_RS485_FRAME_ID])[:-1]
    hub_config = full_config.get_config_for_path(hub_path)
    if CONF_COMMAND_FORMAT not in hub_config:
        raise cv.Invalid(
            "rs485_frame button 'command' requires the referenced hub to have a "
            "'command_format:' block. Add one to the hub, add 'command_format:' directly "
            "to this button, or use the raw 'frame_type' + 'payload' form instead."
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    if CONF_COMMAND in config:
        var = await button.new_button(config, hub, config[CONF_COMMAND])
        if CONF_COMMAND_FORMAT in config:
            cf = config[CONF_COMMAND_FORMAT]
            cg.add(
                var.set_command_format(
                    cf[CONF_PREAMBLE],
                    cf[CONF_COMMAND_SIZE],
                    cf[CONF_COMMAND_ENDIAN] == "big",
                    cf[CONF_COMMAND_REPEAT],
                    cf[CONF_POSTAMBLE],
                )
            )
    else:
        await button.new_button(
            config, hub, config[CONF_FRAME_TYPE], config[CONF_PAYLOAD]
        )
