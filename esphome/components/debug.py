import esphome.config_validation as cv
from esphome.cpp_generator import add
from esphome.cpp_types import App

DEPENDENCIES = ['logger']

CONFIG_SCHEMA = cv.Schema({})


def to_code(config):
    add(App.make_debug_component())


BUILD_FLAGS = '-DUSE_DEBUG_COMPONENT'
