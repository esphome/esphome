from esphome import automation
import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PAYLOAD, CONF_VALUE
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    COMMAND_FORMAT_SCHEMA,
    CONF_COMMAND_FORMAT,
    CONF_FRAME_TYPE,
    CONF_MAX_FRAME_LENGTH,
    CONF_RS485_FRAME_ID,
    CONF_VALUE_ELEMENT_BYTES,
    DOC_COMMAND_FORMAT_URL,
    MAX_COMMAND_VALUES,
    MAX_FRAME_LENGTH_UPPER,
    MIN_FRAME_LENGTH,
    RS485FrameHub,
    encode_command,
    validate_frame_type,
    validate_value_element_bytes,
)

AUTO_LOAD = ["rs485_frame"]


def _validate_button(config: ConfigType) -> ConfigType:
    has_value = CONF_VALUE in config
    if (CONF_FRAME_TYPE in config) != (CONF_PAYLOAD in config):
        raise cv.Invalid(
            "rs485_frame button raw form requires both 'frame_type' and 'payload'."
        )
    if has_value == (CONF_FRAME_TYPE in config):
        raise cv.Invalid(
            "rs485_frame button requires either 'value' or 'frame_type' + 'payload', not both."
        )
    if not has_value and (
        CONF_COMMAND_FORMAT in config or CONF_VALUE_ELEMENT_BYTES in config
    ):
        raise cv.Invalid(
            "rs485_frame button: 'command_format' and 'value_element_bytes' require 'value'."
        )
    if CONF_COMMAND_FORMAT in config and CONF_VALUE_ELEMENT_BYTES in config:
        raise cv.Invalid(
            "rs485_frame button: use 'value_element_bytes' or 'command_format', not both."
        )
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(button.ApplyButton).extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
            cv.Optional(CONF_VALUE): cv.All(
                cv.ensure_list(cv.hex_uint32_t),
                cv.Length(min=1, max=MAX_COMMAND_VALUES),
            ),
            cv.Optional(CONF_VALUE_ELEMENT_BYTES): validate_value_element_bytes,
            cv.Optional(CONF_FRAME_TYPE): validate_frame_type,
            cv.Optional(CONF_PAYLOAD): cv.All(
                cv.ensure_list(cv.hex_uint8_t), cv.Length(max=MAX_FRAME_LENGTH_UPPER)
            ),
            cv.Optional(CONF_COMMAND_FORMAT): COMMAND_FORMAT_SCHEMA,
        }
    ),
    _validate_button,
)


def _resolve_frame(config: ConfigType, hub_config: ConfigType) -> list[int]:
    """The unframed bytes the button sends."""
    if CONF_VALUE not in config:
        return list(config[CONF_FRAME_TYPE]) + list(config[CONF_PAYLOAD])
    command_format = config.get(
        CONF_COMMAND_FORMAT, hub_config.get(CONF_COMMAND_FORMAT)
    )
    return encode_command(
        config[CONF_VALUE], command_format, config.get(CONF_VALUE_ELEMENT_BYTES)
    )


def _final_validate(config: ConfigType) -> ConfigType:
    hub_config = fv.get_declaring_config(config[CONF_RS485_FRAME_ID])
    if (
        CONF_VALUE in config
        and CONF_COMMAND_FORMAT not in config
        and CONF_COMMAND_FORMAT not in hub_config
    ):
        raise cv.Invalid(
            "rs485_frame button 'value' requires a 'command_format:' block on the button or "
            f"the referenced hub, or use the raw 'frame_type' + 'payload' form. See {DOC_COMMAND_FORMAT_URL}"
        )
    frame = _resolve_frame(config, hub_config)
    if len(frame) < MIN_FRAME_LENGTH:
        raise cv.Invalid(
            f"rs485_frame button resolves to a {len(frame)}-byte frame; frames must be at "
            f"least {MIN_FRAME_LENGTH} bytes, the shortest frame rs485_frame accepts on receive"
        )
    if len(frame) > (max_len := hub_config[CONF_MAX_FRAME_LENGTH]):
        raise cv.Invalid(
            f"rs485_frame button resolves to a {len(frame)}-byte frame, exceeding the "
            f"referenced hub's max_frame_length: ({max_len})"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub_id = config[CONF_RS485_FRAME_ID]
    hub_config = fv.get_declaring_config(hub_id, CORE.config)
    frame = _resolve_frame(config, hub_config)
    table = automation.progmem_bytes(f"{config[CONF_ID]}_frame", frame)
    hub = await cg.get_variable(hub_id)
    await button.sub_buttons(config, parent=hub).create(
        config, None, apply=f"queue_frame({table}, {len(frame)})"
    )
