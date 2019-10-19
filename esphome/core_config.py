import logging
import os
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import ARDUINO_VERSION_ESP32_DEV, ARDUINO_VERSION_ESP8266_DEV, \
    CONF_ARDUINO_VERSION, CONF_BOARD, CONF_BOARD_FLASH_MODE, CONF_BUILD_PATH, \
    CONF_COMMENT, CONF_ESPHOME, CONF_INCLUDES, CONF_LIBRARIES, \
    CONF_NAME, CONF_ON_BOOT, CONF_ON_LOOP, CONF_ON_SHUTDOWN, CONF_PLATFORM, \
    CONF_PLATFORMIO_OPTIONS, CONF_PRIORITY, CONF_TRIGGER_ID, \
    CONF_ESP8266_RESTORE_FROM_FLASH, ARDUINO_VERSION_ESP8266_2_3_0, \
    ARDUINO_VERSION_ESP8266_2_5_0, ARDUINO_VERSION_ESP8266_2_5_1, ARDUINO_VERSION_ESP8266_2_5_2
from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, walk_files
from esphome.pins import ESP8266_FLASH_SIZES, ESP8266_LD_SCRIPTS

_LOGGER = logging.getLogger(__name__)

BUILD_FLASH_MODES = ['qio', 'qout', 'dio', 'dout']
StartupTrigger = cg.esphome_ns.class_('StartupTrigger', cg.Component, automation.Trigger.template())
ShutdownTrigger = cg.esphome_ns.class_('ShutdownTrigger', cg.Component,
                                       automation.Trigger.template())
LoopTrigger = cg.esphome_ns.class_('LoopTrigger', cg.Component,
                                   automation.Trigger.template())

VERSION_REGEX = re.compile(r'^[0-9]+\.[0-9]+\.[0-9]+(?:[ab]\d+)?$')


def validate_board(value):
    if CORE.is_esp8266:
        board_pins = pins.ESP8266_BOARD_PINS
    elif CORE.is_esp32:
        board_pins = pins.ESP32_BOARD_PINS
    else:
        raise NotImplementedError

    if value not in board_pins:
        raise cv.Invalid(u"Could not find board '{}'. Valid boards are {}".format(
            value, u', '.join(sorted(board_pins.keys()))))
    return value


validate_platform = cv.one_of('ESP32', 'ESP8266', upper=True)

PLATFORMIO_ESP8266_LUT = {
    '2.5.2': 'espressif8266@2.2.3',
    '2.5.1': 'espressif8266@2.1.0',
    '2.5.0': 'espressif8266@2.0.1',
    '2.4.2': 'espressif8266@1.8.0',
    '2.4.1': 'espressif8266@1.7.3',
    '2.4.0': 'espressif8266@1.6.0',
    '2.3.0': 'espressif8266@1.5.0',
    'RECOMMENDED': 'espressif8266@2.2.3',
    'LATEST': 'espressif8266',
    'DEV': ARDUINO_VERSION_ESP8266_DEV,
}

PLATFORMIO_ESP32_LUT = {
    '1.0.0': 'espressif32@1.4.0',
    '1.0.1': 'espressif32@1.6.0',
    '1.0.2': 'espressif32@1.9.0',
    '1.0.3': 'espressif32@1.10.0',
    '1.0.4': 'espressif32@1.11.0',
    'RECOMMENDED': 'espressif32@1.11.0',
    'LATEST': 'espressif32',
    'DEV': ARDUINO_VERSION_ESP32_DEV,
}


def validate_arduino_version(value):
    value = cv.string_strict(value)
    value_ = value.upper()
    if CORE.is_esp8266:
        if VERSION_REGEX.match(value) is not None and value_ not in PLATFORMIO_ESP8266_LUT:
            raise cv.Invalid("Unfortunately the arduino framework version '{}' is unsupported "
                             "at this time. You can override this by manually using "
                             "espressif8266@<platformio version>")
        if value_ in PLATFORMIO_ESP8266_LUT:
            return PLATFORMIO_ESP8266_LUT[value_]
        return value
    if CORE.is_esp32:
        if VERSION_REGEX.match(value) is not None and value_ not in PLATFORMIO_ESP32_LUT:
            raise cv.Invalid("Unfortunately the arduino framework version '{}' is unsupported "
                             "at this time. You can override this by manually using "
                             "espressif32@<platformio version>")
        if value_ in PLATFORMIO_ESP32_LUT:
            return PLATFORMIO_ESP32_LUT[value_]
        return value
    raise NotImplementedError


def default_build_path():
    return CORE.name


VALID_INCLUDE_EXTS = {'.h', '.hpp', '.tcc', '.ino', '.cpp', '.c'}


def valid_include(value):
    try:
        return cv.directory(value)
    except cv.Invalid:
        pass
    value = cv.file_(value)
    _, ext = os.path.splitext(value)
    if ext not in VALID_INCLUDE_EXTS:
        raise cv.Invalid(u"Include has invalid file extension {} - valid extensions are {}"
                         u"".format(ext, ', '.join(VALID_INCLUDE_EXTS)))
    return value


CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_NAME): cv.valid_name,
    cv.Required(CONF_PLATFORM): cv.one_of('ESP8266', 'ESP32', upper=True),
    cv.Required(CONF_BOARD): validate_board,
    cv.Optional(CONF_COMMENT): cv.string,
    cv.Optional(CONF_ARDUINO_VERSION, default='recommended'): validate_arduino_version,
    cv.Optional(CONF_BUILD_PATH, default=default_build_path): cv.string,
    cv.Optional(CONF_PLATFORMIO_OPTIONS, default={}): cv.Schema({
        cv.string_strict: cv.Any([cv.string], cv.string),
    }),
    cv.SplitDefault(CONF_ESP8266_RESTORE_FROM_FLASH, esp8266=False): cv.All(cv.only_on_esp8266,
                                                                            cv.boolean),

    cv.SplitDefault(CONF_BOARD_FLASH_MODE, esp8266='dout'): cv.one_of(*BUILD_FLASH_MODES,
                                                                      lower=True),
    cv.Optional(CONF_ON_BOOT): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StartupTrigger),
        cv.Optional(CONF_PRIORITY, default=600.0): cv.float_,
    }),
    cv.Optional(CONF_ON_SHUTDOWN): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ShutdownTrigger),
    }),
    cv.Optional(CONF_ON_LOOP): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LoopTrigger),
    }),
    cv.Optional(CONF_INCLUDES, default=[]): cv.ensure_list(valid_include),
    cv.Optional(CONF_LIBRARIES, default=[]): cv.ensure_list(cv.string_strict),

    cv.Optional('esphome_core_version'): cv.invalid("The esphome_core_version option has been "
                                                    "removed in 1.13 - the esphome core source "
                                                    "files are now bundled with ESPHome.")
})

PRELOAD_CONFIG_SCHEMA = cv.Schema({
    cv.Required(CONF_NAME): cv.valid_name,
    cv.Required(CONF_PLATFORM): validate_platform,
}, extra=cv.ALLOW_EXTRA)

PRELOAD_CONFIG_SCHEMA2 = PRELOAD_CONFIG_SCHEMA.extend({
    cv.Required(CONF_BOARD): validate_board,
    cv.Optional(CONF_BUILD_PATH, default=default_build_path): cv.string,
})


def preload_core_config(config):
    core_key = 'esphome'
    if 'esphomeyaml' in config:
        _LOGGER.warning("The esphomeyaml section has been renamed to esphome in 1.11.0. "
                        "Please replace 'esphomeyaml:' in your configuration with 'esphome:'.")
        config[CONF_ESPHOME] = config.pop('esphomeyaml')
        core_key = 'esphomeyaml'
    if CONF_ESPHOME not in config:
        raise cv.RequiredFieldInvalid("required key not provided", CONF_ESPHOME)
    with cv.prepend_path(core_key):
        out = PRELOAD_CONFIG_SCHEMA(config[CONF_ESPHOME])
    CORE.name = out[CONF_NAME]
    CORE.esp_platform = out[CONF_PLATFORM]
    with cv.prepend_path(core_key):
        out2 = PRELOAD_CONFIG_SCHEMA2(config[CONF_ESPHOME])
    CORE.board = out2[CONF_BOARD]
    CORE.build_path = CORE.relative_config_path(out2[CONF_BUILD_PATH])


def include_file(path, basename):
    parts = basename.split(os.path.sep)
    dst = CORE.relative_src_path(*parts)
    copy_file_if_changed(path, dst)

    _, ext = os.path.splitext(path)
    if ext in ['.h', '.hpp', '.tcc']:
        # Header, add include statement
        cg.add_global(cg.RawStatement(u'#include "{}"'.format(basename)))


@coroutine_with_priority(-1000.0)
def add_includes(includes):
    # Add includes at the very end, so that the included files can access global variables
    for include in includes:
        path = CORE.relative_config_path(include)
        if os.path.isdir(path):
            # Directory, copy tree
            for p in walk_files(path):
                basename = os.path.relpath(p, os.path.dirname(path))
                include_file(p, basename)
        else:
            # Copy file
            basename = os.path.basename(path)
            include_file(path, basename)


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(cg.global_ns.namespace('esphome').using)
    cg.add(cg.App.pre_setup(config[CONF_NAME], cg.RawExpression('__DATE__ ", " __TIME__')))

    for conf in config.get(CONF_ON_BOOT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf.get(CONF_PRIORITY))
        yield cg.register_component(trigger, conf)
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SHUTDOWN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield cg.register_component(trigger, conf)
        yield automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_LOOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        yield cg.register_component(trigger, conf)
        yield automation.build_automation(trigger, [], conf)

    # Build flags
    if CORE.is_esp8266 and CORE.board in ESP8266_FLASH_SIZES and \
            CORE.arduino_version != ARDUINO_VERSION_ESP8266_2_3_0:
        flash_size = ESP8266_FLASH_SIZES[CORE.board]
        ld_scripts = ESP8266_LD_SCRIPTS[flash_size]
        ld_script = None

        if CORE.arduino_version in ('espressif8266@1.8.0', 'espressif8266@1.7.3',
                                    'espressif8266@1.6.0'):
            ld_script = ld_scripts[0]
        elif CORE.arduino_version in (ARDUINO_VERSION_ESP8266_DEV, ARDUINO_VERSION_ESP8266_2_5_0,
                                      ARDUINO_VERSION_ESP8266_2_5_1, ARDUINO_VERSION_ESP8266_2_5_2):
            ld_script = ld_scripts[1]

        if ld_script is not None:
            cg.add_build_flag('-Wl,-T{}'.format(ld_script))

    cg.add_build_flag('-fno-exceptions')

    # Libraries
    if CORE.is_esp32:
        cg.add_library('ESPmDNS', None)
    elif CORE.is_esp8266:
        cg.add_library('ESP8266WiFi', None)
        cg.add_library('ESP8266mDNS', None)

    for lib in config[CONF_LIBRARIES]:
        if '@' in lib:
            name, vers = lib.split('@', 1)
            cg.add_library(name, vers)
        else:
            cg.add_library(lib, None)

    cg.add_build_flag('-Wno-unused-variable')
    cg.add_build_flag('-Wno-unused-but-set-variable')
    cg.add_build_flag('-Wno-sign-compare')
    if config.get(CONF_ESP8266_RESTORE_FROM_FLASH, False):
        cg.add_define('USE_ESP8266_PREFERENCES_FLASH')

    if config[CONF_INCLUDES]:
        CORE.add_job(add_includes, config[CONF_INCLUDES])
