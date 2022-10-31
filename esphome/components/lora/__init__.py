from enum import Enum

import esphome.codegen as cg
import esphome.config_validation as cv

IS_PLATFORM_COMPONENT = True

CONF_LORA_ID = "lora_id"
CONF_SEND_TO_LORA = "send_to_lora"
CONF_RECEIVE_FROM_LORA = "receive_from_lora"
CONF_SYNC_WORD = "sync_word"
CONF_LORA_NAME = "lora_name"

# Base
lora_ns = cg.esphome_ns.namespace("lora")
Lora = lora_ns.class_("Lora", cg.Component)

LoraComponent = lora_ns.class_("LoraComponent", cg.Component)

LORA_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LoraComponent),
        cv.Required(CONF_SYNC_WORD): cv.hex_int_range(min=0x00, max=0xFF),
    }
)


class LoraComponentType(Enum):
    SENSOR = 0
    SWITCH = 1
    BINARY_SENSOR = 2
    TEXT_SENSOR = 3


async def register_lora_component(var, config, type):
    send_to_lora = config[CONF_SEND_TO_LORA]
    receive_from_lora = config[CONF_RECEIVE_FROM_LORA]

    if send_to_lora or receive_from_lora:
        parent = await cg.get_variable(config[CONF_LORA_ID])
        lora_name = ""
        if CONF_LORA_NAME in config:
            lora_name = config[CONF_LORA_NAME]

        function_mapping = {
            LoraComponentType.SENSOR: parent.register_sensor,
            LoraComponentType.SWITCH: parent.register_switch,
            LoraComponentType.BINARY_SENSOR: parent.register_binary_sensor,
            LoraComponentType.TEXT_SENSOR: parent.register_text_sensor,
        }

        assert type in function_mapping
        cg.add(function_mapping[type](var, send_to_lora, receive_from_lora, lora_name))
