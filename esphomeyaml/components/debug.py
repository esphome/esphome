import voluptuous as vol

from esphomeyaml.helpers import App, add

DEPENDENCIES = ['logger']

CONFIG_SCHEMA = vol.Schema({})


def to_code(config):
    add(App.make_debug_component())


BUILD_FLAGS = '-DUSE_DEBUG_COMPONENT'
