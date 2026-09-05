import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_PAYLOAD, CONF_VALUE
import esphome.final_validate as fv

from .. import (
    COMMAND_FORMAT_SCHEMA,
    CONF_COMMAND_FORMAT,
    CONF_ENDIAN,
    CONF_FRAME_TYPE,
    CONF_POSTAMBLE,
    CONF_PREAMBLE,
    CONF_RS485_FRAME_ID,
    CONF_VALUE_ELEMENT_BYTES,
    DOC_COMMAND_FORMAT_URL,
    MAX_COMMAND_VALUES,
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
    has_value = CONF_VALUE in config
    has_frame_type = CONF_FRAME_TYPE in config
    has_payload = CONF_PAYLOAD in config
    has_cmd_format = CONF_COMMAND_FORMAT in config
    has_element_bytes = CONF_VALUE_ELEMENT_BYTES in config

    if has_element_bytes and not has_value:
        raise cv.Invalid("rs485_frame button: 'value_element_bytes' requires 'value'.")
    if has_element_bytes and has_cmd_format:
        raise cv.Invalid(
            "rs485_frame button: 'value_element_bytes' and 'command_format' both specify "
            "element byte width — use one or the other, not both."
        )
    if has_cmd_format and not has_value:
        raise cv.Invalid(
            "rs485_frame button: 'command_format' requires 'value'. "
            "Per-button command_format is only meaningful with the 'value:' form."
        )
    if has_cmd_format and (has_frame_type or has_payload):
        raise cv.Invalid(
            "rs485_frame button: 'command_format' cannot be combined with 'frame_type'/'payload'."
        )
    if has_value and (has_frame_type or has_payload):
        raise cv.Invalid(
            "rs485_frame button: 'value' is mutually exclusive with 'frame_type'/'payload'. "
            "Use 'value' to encode via the hub's command_format, or use 'frame_type' + "
            "'payload' to emit a raw frame."
        )
    if has_frame_type != has_payload:
        raise cv.Invalid(
            "rs485_frame button raw form requires both 'frame_type' and 'payload'."
        )
    if not has_value and not has_frame_type:
        raise cv.Invalid(
            "rs485_frame button requires either 'value' or 'frame_type' + 'payload'."
        )
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(RS485FrameButton).extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
            # A single hex value, or a list of up to MAX_COMMAND_VALUES values encoded
            # back-to-back in one frame. Requires the hub (or button) to have a command_format.
            cv.Optional(CONF_VALUE): cv.All(
                cv.ensure_list(validate_u32), cv.Length(min=1, max=MAX_COMMAND_VALUES)
            ),
            # Lightweight per-button override: use this hub's preamble/endian/postamble but a
            # different element byte width for this one button. Mutually exclusive with
            # command_format (which provides a full format override including element width).
            cv.Optional(CONF_VALUE_ELEMENT_BYTES): cv.one_of(1, 2, 3, 4, int=True),
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
    # The `value:` form requires a command_format somewhere — either on the button itself
    # (command_format: or value_element_bytes:) or on the hub. If the button supplies its own
    # full command_format, skip the hub check.
    if CONF_VALUE not in config:
        return config
    if CONF_COMMAND_FORMAT in config:
        return config

    full_config = fv.full_config.get()
    hub_path = full_config.get_path_for_id(config[CONF_RS485_FRAME_ID])[:-1]
    hub_config = full_config.get_config_for_path(hub_path)
    if CONF_COMMAND_FORMAT not in hub_config:
        # value_element_bytes on the button provides element width but still needs the hub's
        # preamble/endian/postamble, so the hub must have a command_format.
        raise cv.Invalid(
            "rs485_frame button 'value' requires the referenced hub to have a "
            "'command_format:' block (missing on the hub). Add one to the hub, add "
            "'command_format:' directly to this button, or use the raw 'frame_type' + "
            f"'payload' form instead. See {DOC_COMMAND_FORMAT_URL}"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    hub = await cg.get_variable(config[CONF_RS485_FRAME_ID])
    if CONF_VALUE in config:
        var = await button.new_button(config, hub, config[CONF_VALUE])
        if CONF_COMMAND_FORMAT in config:
            cf = config[CONF_COMMAND_FORMAT]
            cg.add(
                var.set_command_format(
                    cf[CONF_PREAMBLE],
                    cf[CONF_VALUE_ELEMENT_BYTES],
                    cf[CONF_ENDIAN] == "big",
                    cf[CONF_POSTAMBLE],
                )
            )
        elif CONF_VALUE_ELEMENT_BYTES in config:
            cg.add(var.set_value_element_bytes(config[CONF_VALUE_ELEMENT_BYTES]))
    else:
        await button.new_button(
            config, hub, config[CONF_FRAME_TYPE], config[CONF_PAYLOAD]
        )
