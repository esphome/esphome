
import esphome.codegen as cg
import esphome.config_validation as cv


# rom esphome.const import CONF_ID

IS_PLATFORM_COMPONENT = True

CONF_LORA_ID = "lora_id"
CONF_SEND_TO_LORA = "send_to_lora"
CONF_RECEIVE_FROM_LORA = "receive_from_lora"
CONF_SYNC_WORD = "sync_word"
CONF_LORA_NAME = "lora_name"

# Base
lora_ns = cg.esphome_ns.namespace('lora')
Lora = lora_ns.class_('Lora', cg.Component)
LoraPtr = Lora.operator('ptr')

LoraComponent = lora_ns.class_(
    'LoraComponent', cg.Component)


LORA_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(LoraComponent),
    cv.Required(CONF_SYNC_WORD): cv.hex_int_range(min=0x00, max=0xFF)
})


def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))


# def to_code(config):
    # dump(config)
    # cg.new_Pvariable(config[CONF_ID])
