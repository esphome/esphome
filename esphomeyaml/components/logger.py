import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_BAUD_RATE, CONF_ID, CONF_LEVEL, CONF_LOGGER, CONF_LOGS, \
    CONF_TX_BUFFER_SIZE
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, RawExpression, add

LOG_LEVELS = ['NONE', 'ERROR', 'WARN', 'INFO', 'DEBUG', 'VERBOSE', 'VERY_VERBOSE']

# pylint: disable=invalid-name
is_log_level = vol.All(vol.Upper, vol.Any(*LOG_LEVELS))

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(CONF_LOGGER): cv.register_variable_id,
    vol.Optional(CONF_BAUD_RATE): cv.positive_int,
    vol.Optional(CONF_TX_BUFFER_SIZE): cv.positive_int,
    vol.Optional(CONF_LEVEL): is_log_level,
    vol.Optional(CONF_LOGS): vol.Schema({
        cv.string: is_log_level,
    })
})


def esphomelib_log_level(level):
    return u'ESPHOMELIB_LOG_LEVEL_{}'.format(level)


def exp_log_level(level):
    return RawExpression(esphomelib_log_level(level))


def to_code(config):
    rhs = App.init_log(config.get(CONF_BAUD_RATE))
    log = Pvariable(u'LogComponent', config[CONF_ID], rhs)
    if CONF_TX_BUFFER_SIZE in config:
        add(log.set_tx_buffer_size(config[CONF_TX_BUFFER_SIZE]))
    if CONF_LEVEL in config:
        add(log.set_global_log_level(exp_log_level(config[CONF_LEVEL])))
    for tag, level in config.get(CONF_LOGS, {}).iteritems():
        global_level = config.get(CONF_LEVEL, 'DEBUG')
        if LOG_LEVELS.index(level) > LOG_LEVELS.index(global_level):
            raise ESPHomeYAMLError(u"The local log level {} for {} must be less severe than the "
                                   u"global log level {}.".format(level, tag, global_level))
        add(log.set_log_level(tag, exp_log_level(level)))


def required_build_flags(config):
    if CONF_LEVEL in config:
        return u'-DESPHOMELIB_LOG_LEVEL={}'.format(esphomelib_log_level(config[CONF_LEVEL]))
    return None
