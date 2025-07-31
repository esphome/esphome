import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    'The "custom" component has been removed. Consider conversion to an external component.\nhttps://esphome.io/guides/contributing#a-note-about-custom-components'
)
