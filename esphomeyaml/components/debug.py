import voluptuous as vol

from esphomeyaml.cpp_generator import add
from esphomeyaml.cpp_types import App

DEPENDENCIES = ['logger']

CONFIG_SCHEMA = vol.Schema({})


def to_code(config):
    add(App.make_debug_component())


BUILD_FLAGS = '-DUSE_DEBUG_COMPONENT'
