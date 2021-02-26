import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID

IS_PLATFORM_COMPONENT = True
MULTI_CONF = True

# Base
lora_ns = cg.esphome_ns.namespace('lora')
Lora = lora_ns.class_('Lora', cg.Nameable)
LoraPtr = Lora.operator('ptr')


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Lora),
})


def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))


def to_code(config):
    # dump(config)
    cg.new_Pvariable(config[CONF_ID])
