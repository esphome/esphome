from esphome.core import coroutine
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


def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))


@coroutine
def register_lora_component(var, config, type):
    send_to_lora = config[CONF_SEND_TO_LORA] is True
    receive_from_lora = config[CONF_RECEIVE_FROM_LORA] is True
    if send_to_lora is True or receive_from_lora is True:
        parent = yield cg.get_variable(config[CONF_LORA_ID])
        lora_name = ""
        if CONF_LORA_NAME in config:
            lora_name = config[CONF_LORA_NAME]

        if type == 0:
            cg.add(
                parent.register_sensor(var, send_to_lora, receive_from_lora, lora_name)
            )

        elif type == 1:
            cg.add(
                parent.register_switch(var, send_to_lora, receive_from_lora, lora_name)
            )

        elif type == 2:
            cg.add(
                parent.register_binary_sensor(
                    var, send_to_lora, receive_from_lora, lora_name
                )
            )

        elif type == 3:
            cg.add(
                parent.register_text_sensor(
                    var, send_to_lora, receive_from_lora, lora_name
                )
            )
