import esphome.config_validation as cv
from esphome.config_validation import Schema

# This external component requests auto-loading a component that is not present

AUTO_LOAD: list[str] = ["absent_component_xyz"]

CONFIG_SCHEMA: Schema = cv.Schema({})
