import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_BAUD_RATE, CONF_ID, CONF_LEVEL, CONF_LOGS, \
    CONF_TX_BUFFER_SIZE
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, global_ns

LOG_LEVELS = {
    'NONE': global_ns.ESPHOMELIB_LOG_LEVEL_NONE,
    'ERROR': global_ns.ESPHOMELIB_LOG_LEVEL_ERROR,
    'WARN': global_ns.ESPHOMELIB_LOG_LEVEL_WARN,
    'INFO': global_ns.ESPHOMELIB_LOG_LEVEL_INFO,
    'DEBUG': global_ns.ESPHOMELIB_LOG_LEVEL_DEBUG,
    'VERBOSE': global_ns.ESPHOMELIB_LOG_LEVEL_VERBOSE,
    'VERY_VERBOSE': global_ns.ESPHOMELIB_LOG_LEVEL_VERY_VERBOSE,
}

LOG_LEVEL_SEVERITY = ['NONE', 'ERROR', 'WARN', 'INFO', 'DEBUG', 'VERBOSE', 'VERY_VERBOSE']

# pylint: disable=invalid-name
is_log_level = vol.All(vol.Upper, cv.one_of(*LOG_LEVELS))


def validate_local_no_higher_than_global(value):
    global_level = value.get(CONF_LEVEL, 'DEBUG')
    for tag, level in value.get(CONF_LOGS, {}).iteritems():
        if LOG_LEVEL_SEVERITY.index(level) > LOG_LEVEL_SEVERITY.index(global_level):
            raise ESPHomeYAMLError(u"The local log level {} for {} must be less severe than the "
                                   u"global log level {}.".format(level, tag, global_level))
    return value


LogComponent = esphomelib_ns.LogComponent

CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(LogComponent),
    vol.Optional(CONF_BAUD_RATE): cv.positive_int,
    vol.Optional(CONF_TX_BUFFER_SIZE): cv.validate_bytes,
    vol.Optional(CONF_LEVEL): is_log_level,
    vol.Optional(CONF_LOGS): vol.Schema({
        cv.string: is_log_level,
    })
}), validate_local_no_higher_than_global)


def to_code(config):
    rhs = App.init_log(config.get(CONF_BAUD_RATE))
    log = Pvariable(config[CONF_ID], rhs)
    if CONF_TX_BUFFER_SIZE in config:
        add(log.set_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))
    if CONF_LEVEL in config:
        add(log.set_global_log_level(LOG_LEVELS[config[CONF_LEVEL]]))
    for tag, level in config.get(CONF_LOGS, {}).iteritems():
        add(log.set_log_level(tag, LOG_LEVELS[level]))


def required_build_flags(config):
    if CONF_LEVEL in config:
        return u'-DESPHOMELIB_LOG_LEVEL={}'.format(str(LOG_LEVELS[config[CONF_LEVEL]]))
    return None
