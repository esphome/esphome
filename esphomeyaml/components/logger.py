import re

import voluptuous as vol

from esphomeyaml.automation import ACTION_REGISTRY, LambdaAction
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ARGS, CONF_BAUD_RATE, CONF_FORMAT, CONF_ID, CONF_LEVEL, \
    CONF_LOGS, CONF_TAG, CONF_TX_BUFFER_SIZE
from esphomeyaml.core import EsphomeyamlError, Lambda, CORE
from esphomeyaml.cpp_generator import Pvariable, RawExpression, add, process_lambda, statement
from esphomeyaml.cpp_types import App, Component, esphomelib_ns, global_ns, void

from esphomeyaml.py_compat import text_type

LOG_LEVELS = {
    'NONE': global_ns.ESPHOMELIB_LOG_LEVEL_NONE,
    'ERROR': global_ns.ESPHOMELIB_LOG_LEVEL_ERROR,
    'WARN': global_ns.ESPHOMELIB_LOG_LEVEL_WARN,
    'INFO': global_ns.ESPHOMELIB_LOG_LEVEL_INFO,
    'DEBUG': global_ns.ESPHOMELIB_LOG_LEVEL_DEBUG,
    'VERBOSE': global_ns.ESPHOMELIB_LOG_LEVEL_VERBOSE,
    'VERY_VERBOSE': global_ns.ESPHOMELIB_LOG_LEVEL_VERY_VERBOSE,
}

LOG_LEVEL_TO_ESP_LOG = {
    'ERROR': global_ns.ESP_LOGE,
    'WARN': global_ns.ESP_LOGW,
    'INFO': global_ns.ESP_LOGI,
    'DEBUG': global_ns.ESP_LOGD,
    'VERBOSE': global_ns.ESP_LOGV,
    'VERY_VERBOSE': global_ns.ESP_LOGVV,
}

LOG_LEVEL_SEVERITY = ['NONE', 'ERROR', 'WARN', 'INFO', 'DEBUG', 'VERBOSE', 'VERY_VERBOSE']

# pylint: disable=invalid-name
is_log_level = cv.one_of(*LOG_LEVELS, upper=True)


def validate_local_no_higher_than_global(value):
    global_level = value.get(CONF_LEVEL, 'DEBUG')
    for tag, level in value.get(CONF_LOGS, {}).items():
        if LOG_LEVEL_SEVERITY.index(level) > LOG_LEVEL_SEVERITY.index(global_level):
            raise EsphomeyamlError(u"The local log level {} for {} must be less severe than the "
                                   u"global log level {}.".format(level, tag, global_level))
    return value


LogComponent = esphomelib_ns.class_('LogComponent', Component)

CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(LogComponent),
    vol.Optional(CONF_BAUD_RATE, default=115200): cv.positive_int,
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
    for tag, level in config.get(CONF_LOGS, {}).items():
        add(log.set_log_level(tag, LOG_LEVELS[level]))


def required_build_flags(config):
    flags = []
    if CONF_LEVEL in config:
        flags.append(u'-DESPHOMELIB_LOG_LEVEL={}'.format(str(LOG_LEVELS[config[CONF_LEVEL]])))
        this_severity = LOG_LEVEL_SEVERITY.index(config[CONF_LEVEL])
        verbose_severity = LOG_LEVEL_SEVERITY.index('VERBOSE')
        is_at_least_verbose = this_severity >= verbose_severity
        has_serial_logging = config.get(CONF_BAUD_RATE) != 0
        if CORE.is_esp8266 and has_serial_logging and is_at_least_verbose:
            flags.append(u"-DDEBUG_ESP_PORT=Serial")
            flags.append(u"-DLWIP_DEBUG")
            DEBUG_COMPONENTS = {
                'HTTP_CLIENT',
                'HTTP_SERVER',
                'HTTP_UPDATE',
                'OTA',
                'SSL',
                'TLS_MEM',
                'UPDATER',
                'WIFI',
            }
            for comp in DEBUG_COMPONENTS:
                flags.append(u"-DDEBUG_ESP_{}".format(comp))
        if CORE.is_esp32 and is_at_least_verbose:
            flags.append('-DCORE_DEBUG_LEVEL=5')

    return flags


def maybe_simple_message(schema):
    def validator(value):
        if isinstance(value, dict):
            return vol.Schema(schema)(value)
        return vol.Schema(schema)({CONF_FORMAT: value})

    return validator


def validate_printf(value):
    # https://stackoverflow.com/questions/30011379/how-can-i-parse-a-c-format-string-in-python
    # pylint: disable=anomalous-backslash-in-string
    cfmt = u"""\
    (                                  # start of capture group 1
    %                                  # literal "%"
    (?:                                # first option
    (?:[-+0 #]{0,5})                   # optional flags
    (?:\d+|\*)?                        # width
    (?:\.(?:\d+|\*))?                  # precision
    (?:h|l|ll|w|I|I32|I64)?            # size
    [cCdiouxXeEfgGaAnpsSZ]             # type
    ) |                                # OR
    %%)                                # literal "%%"
    """  # noqa
    matches = re.findall(cfmt, value[CONF_FORMAT], flags=re.X)
    if len(matches) != len(value[CONF_ARGS]):
        raise vol.Invalid(u"Found {} printf-patterns ({}), but {} args were given!"
                          u"".format(len(matches), u', '.join(matches), len(value[CONF_ARGS])))
    return value


CONF_LOGGER_LOG = 'logger.log'
LOGGER_LOG_ACTION_SCHEMA = vol.All(maybe_simple_message({
    vol.Required(CONF_FORMAT): cv.string,
    vol.Optional(CONF_ARGS, default=list): cv.ensure_list(cv.lambda_),
    vol.Optional(CONF_LEVEL, default="DEBUG"): cv.one_of(*LOG_LEVEL_TO_ESP_LOG, upper=True),
    vol.Optional(CONF_TAG, default="main"): cv.string,
}), validate_printf)


@ACTION_REGISTRY.register(CONF_LOGGER_LOG, LOGGER_LOG_ACTION_SCHEMA)
def logger_log_action_to_code(config, action_id, arg_type, template_arg):
    esp_log = LOG_LEVEL_TO_ESP_LOG[config[CONF_LEVEL]]
    args = [RawExpression(text_type(x)) for x in config[CONF_ARGS]]

    text = text_type(statement(esp_log(config[CONF_TAG], config[CONF_FORMAT], *args)))

    for lambda_ in process_lambda(Lambda(text), [(arg_type, 'x')], return_type=void):
        yield None
    rhs = LambdaAction.new(template_arg, lambda_)
    type = LambdaAction.template(template_arg)
    yield Pvariable(action_id, rhs, type=type)
