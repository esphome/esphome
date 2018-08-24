# coding=utf-8
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_LAMBDA, CONF_ROTATION, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import add, add_job, esphomelib_ns

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

display_ns = esphomelib_ns.namespace('display')
DisplayBuffer = display_ns.DisplayBuffer
DisplayBufferRef = DisplayBuffer.operator('ref')

DISPLAY_ROTATIONS = {
    0: display_ns.DISPLAY_ROTATION_0_DEGREES,
    90: display_ns.DISPLAY_ROTATION_90_DEGREES,
    180: display_ns.DISPLAY_ROTATION_180_DEGREES,
    270: display_ns.DISPLAY_ROTATION_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith(u"°"):
        value = value[:-1]
    try:
        value = int(value)
    except ValueError:
        raise vol.Invalid(u"Expected integer for rotation")
    return cv.one_of(*DISPLAY_ROTATIONS)(value)


BASIC_DISPLAY_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_LAMBDA): cv.lambda_,
})

FULL_DISPLAY_PLATFORM_SCHEMA = BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_ROTATION): validate_rotation,
})


def setup_display_core_(display_var, config):
    if CONF_UPDATE_INTERVAL in config:
        add(display_var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    if CONF_ROTATION in config:
        add(display_var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))


def setup_display(display_var, config):
    add_job(setup_display_core_, display_var, config)


BUILD_FLAGS = '-DUSE_DISPLAY'
