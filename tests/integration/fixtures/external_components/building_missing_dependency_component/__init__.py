import esphome.config_validation as cv
from esphome.config_validation import Schema

# This external component declares a dependency on a component that is not present.

DEPENDENCIES: list[str] = ["absent_component_xyz"]

CONFIG_SCHEMA: Schema = cv.Schema({})
