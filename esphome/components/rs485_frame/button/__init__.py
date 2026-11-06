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
    RS485FrameHub,
    check_frame_length,
    encode_command,
    validate_frame_type,
    validate_value_element_bytes,
)

AUTO_LOAD = ["rs485_frame"]

_RAW_MSG = "rs485_frame button raw form requires both 'frame_type' and 'payload'."
_ENCODING_MSG = (
    "rs485_frame button: set value_element_bytes or command_format, not both"
)

CONFIG_SCHEMA = cv.All(
    button.button_schema(button.ApplyButton).extend(
        {
            cv.GenerateID(CONF_RS485_FRAME_ID): cv.use_id(RS485FrameHub),
            cv.Optional(CONF_VALUE): cv.All(
                cv.ensure_list(cv.hex_uint32_t),
                cv.Length(min=1, max=MAX_COMMAND_VALUES),
            ),
            cv.Exclusive(
                CONF_VALUE_ELEMENT_BYTES, "encoding", msg=_ENCODING_MSG
            ): validate_value_element_bytes,
            cv.Exclusive(
                CONF_COMMAND_FORMAT, "encoding", msg=_ENCODING_MSG
            ): COMMAND_FORMAT_SCHEMA,
            cv.Inclusive(CONF_FRAME_TYPE, "raw", msg=_RAW_MSG): validate_frame_type,
            cv.Inclusive(CONF_PAYLOAD, "raw", msg=_RAW_MSG): cv.ensure_list(
                cv.hex_uint8_t
            ),
        }
    ),
    # 'value' or the raw form; command_format and value_element_bytes only encode 'value'
    cv.has_exactly_one_key(CONF_VALUE, CONF_FRAME_TYPE),
    cv.has_at_most_one_key(CONF_FRAME_TYPE, CONF_COMMAND_FORMAT),
    cv.has_at_most_one_key(CONF_FRAME_TYPE, CONF_VALUE_ELEMENT_BYTES),
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
            f"rs485_frame button 'value' needs a command_format on the button or hub. See {DOC_COMMAND_FORMAT_URL}"
        )
    frame = _resolve_frame(config, hub_config)
    check_frame_length(
        "rs485_frame button", len(frame), hub_config[CONF_MAX_FRAME_LENGTH]
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
