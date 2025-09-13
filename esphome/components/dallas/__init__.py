import esphome.config_validation as cv

MULTI_CONF = True

CONFIG_SCHEMA = cv.invalid(
    'The "dallas" component has been replaced by the "one_wire" component.\nhttps://esphome.io/components/one_wire'
)
